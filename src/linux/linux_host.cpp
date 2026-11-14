#include "linux_host.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/raster_thread.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/surface.hpp>

#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <mutex>

namespace GPU = ::systems::leal::campello_gpu;

namespace systems::leal::campello_widgets::linux_host
{
    namespace
    {
        std::shared_ptr<GPU::Device>       g_device;
        int                                g_wake_fd = -1;
        std::mutex                         g_tasks_mutex;
        std::vector<std::function<void()>> g_tasks;
        std::vector<LinuxWindow*>          g_windows;
    }

    // ----------------------------------------------------------------------
    // App-level host
    // ----------------------------------------------------------------------

    bool init(std::shared_ptr<GPU::Device> device)
    {
        g_device  = std::move(device);
        g_wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (g_wake_fd < 0)
        {
            std::cerr << "[Linux] eventfd() failed\n";
            return false;
        }
        return true;
    }

    void shutdown()
    {
        runTasks();
        if (g_wake_fd >= 0) close(g_wake_fd);
        g_wake_fd = -1;
        g_device.reset();
    }

    std::shared_ptr<GPU::Device> device() { return g_device; }

    void postTask(std::function<void()> task)
    {
        if (!task) return;
        {
            std::lock_guard<std::mutex> lock(g_tasks_mutex);
            g_tasks.push_back(std::move(task));
        }
        wake();
    }

    void runTasks()
    {
        // Tasks may post more tasks: run batches until the queue stays empty.
        for (;;)
        {
            std::vector<std::function<void()>> batch;
            {
                std::lock_guard<std::mutex> lock(g_tasks_mutex);
                batch.swap(g_tasks);
            }
            if (batch.empty()) return;
            for (auto& task : batch) task();
        }
    }

    int wakeFd() { return g_wake_fd; }

    void wake()
    {
        if (g_wake_fd < 0) return;
        const uint64_t one = 1;
        (void)!write(g_wake_fd, &one, sizeof one);
    }

    void clearWake()
    {
        if (g_wake_fd < 0) return;
        uint64_t value = 0;
        (void)!read(g_wake_fd, &value, sizeof value);
    }

    // ----------------------------------------------------------------------
    // LinuxWindow
    // ----------------------------------------------------------------------

    LinuxWindow::LinuxWindow(View& view)
        : view_(view)
    {
        g_windows.push_back(this);
        view_.setFrameRequestCallback([this] { requestFrame(); });
    }

    LinuxWindow::~LinuxWindow()
    {
        stopRendering();
        if (View::isAlive(&view_))
        {
            view_.setFrameRequestCallback(nullptr);
            view_.pointerDispatcher().setRoot(nullptr);
        }
        root_.reset();
        surface_.reset();
        g_windows.erase(std::remove(g_windows.begin(), g_windows.end(), this), g_windows.end());
    }

    bool LinuxWindow::attachSurface(const GPU::LinuxSurfaceInfo& info)
    {
        if (!g_device) return false;
        GPU::LinuxSurfaceInfo copy = info;
        surface_ = g_device->createSurface(&copy, info.width, info.height);
        return surface_ != nullptr;
    }

    void LinuxWindow::setRoot(std::shared_ptr<RenderBox> root, Color background)
    {
        stopRendering();
        root_ = std::move(root);
        view_.pointerDispatcher().setRoot(root_);
        if (!root_ || !surface_ || !g_device) return;

        renderer_ = std::make_shared<Renderer>(g_device, root_, background);
        // The draw backend's pipelines must match the surface's real format
        // (not every Vulkan surface offers BGRA8).
        renderer_->setDrawBackend(std::make_unique<VulkanDrawBackend>(
            g_device, background, surface_->getPixelFormat()));
        // Renderer::device_pixel_ratio_ is what buildFrame() divides the
        // physical viewport by to get logical layout constraints (it is
        // distinct from MediaQueryData's copy).
        renderer_->setDevicePixelRatio(devicePixelRatio());
        view_.setRenderer(renderer_.get());  // also points the renderer at the View

        // Rasterize (including the GPU submit and present) on a thread of
        // this window's own, so a GPU or compositor stall never blocks the UI
        // thread, which handles input for every window.
        raster_ = std::make_unique<RasterThread>(
            [renderer = renderer_](const FramePackage& pkg) { renderer->rasterFrame(pkg); });
        raster_->start();
        requestFrame();
    }

    void LinuxWindow::stopRendering()
    {
        raster_.reset();  // stops and joins: no raster work in flight below
        if (View::isAlive(&view_) && view_.renderer() == renderer_.get()) view_.setRenderer(nullptr);
        renderer_.reset();
    }

    void LinuxWindow::requestFrame()
    {
        needs_frame_ = true;
        wake();
    }

    bool LinuxWindow::drawFrame()
    {
        if (!needs_frame_ || !renderer_ || !raster_ || !isShown()) return false;
        const uint32_t w = physicalWidth(), h = physicalHeight();
        if (w == 0 || h == 0) return false;
        const auto now = std::chrono::steady_clock::now();
        if (min_interval_.count() > 0 && now - last_frame_ < min_interval_) return false;  // not due yet
        needs_frame_ = false;
        last_frame_  = now;

        // Held for the whole frame: building it runs every window's pending
        // rebuilds (the shared buildScope()), which may replace this window's
        // renderer or destroy the window (a popup closing itself).
        const auto frame_renderer = renderer_;
        const auto frame_surface  = surface_;
        frame_renderer->setDevicePixelRatio(devicePixelRatio());
        auto package = frame_renderer->buildFrame(static_cast<float>(w), static_cast<float>(h));
        if (!package || !isLive(this) || frame_renderer != renderer_ || !raster_) return false;
        // Acquired and presented on this window's raster thread.
        package->surface = frame_surface;
        raster_->submit(std::move(*package));
        return true;
    }

    void LinuxWindow::resizeSurface(uint32_t width, uint32_t height)
    {
        // Only flags the swapchain for rebuilding; the raster thread rebuilds
        // it at its next acquire, under the surface's own lock.
        if (surface_) surface_->resize(width, height);
    }

    void LinuxWindow::notifyMetricsChanged()
    {
        if (renderer_)
        {
            renderer_->setDevicePixelRatio(devicePixelRatio());
            // Repaint everything even if no widget changed: the swapchain is
            // rebuilt at the new size, and the last frame must not stay on
            // screen cropped or stretched.
            renderer_->notePaintRequested();
        }
        requestFrame();
        auto callback = on_metrics_changed;  // may destroy `this`
        if (callback) callback();
    }

    Size LinuxWindow::logicalSize() const
    {
        const float dpr = devicePixelRatio() > 0.0f ? devicePixelRatio() : 1.0f;
        return Size{ static_cast<float>(physicalWidth()) / dpr,
                     static_cast<float>(physicalHeight()) / dpr };
    }

    const std::vector<LinuxWindow*>& LinuxWindow::all() { return g_windows; }

    bool LinuxWindow::isLive(const LinuxWindow* window)
    {
        return std::find(g_windows.begin(), g_windows.end(), window) != g_windows.end();
    }

    LinuxWindow* LinuxWindow::forView(const View* view)
    {
        for (LinuxWindow* w : g_windows)
            if (&w->view_ == view) return w;
        return nullptr;
    }

    int LinuxWindow::msUntilNextFrame()
    {
        int  soonest = -1;
        const auto now = std::chrono::steady_clock::now();
        for (LinuxWindow* w : g_windows)
        {
            if (!w->needs_frame_ || !w->renderer_ || !w->isShown()) continue;
            const auto due  = w->last_frame_ + w->min_interval_;
            const int  wait = due <= now ? 0 : static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(due - now).count()) + 1;
            if (soonest < 0 || wait < soonest) soonest = wait;
        }
        return soonest;
    }

    bool LinuxWindow::anyFramePending()
    {
        for (LinuxWindow* w : g_windows)
            if (w->needs_frame_ && w->renderer_ && w->isShown()) return true;
        return false;
    }

    void LinuxWindow::drawPendingFrames()
    {
        // A copy: a frame can create or destroy windows.
        const std::vector<LinuxWindow*> windows = g_windows;
        for (LinuxWindow* w : windows)
            if (isLive(w)) w->drawFrame();
    }

} // namespace systems::leal::campello_widgets::linux_host
