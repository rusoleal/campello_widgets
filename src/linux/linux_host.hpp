#pragma once

// Linux host internals (Phase 17, multi-window): one LinuxWindow per native
// window (X11 Window or Wayland wl_surface), each with its own View,
// campello_gpu Surface, Renderer and raster thread, all driven by one UI
// thread and one shared, windowless GPU device. The X11 and Wayland runners
// derive their windows from LinuxWindow and own the event loop; this file
// holds what both share. Not public API.

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/size.hpp>

#include <campello_gpu/platform/linux_surface.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class Surface;
}

namespace systems::leal::campello_widgets
{
    class View;
    class Renderer;
    class RenderBox;
    class RasterThread;

    namespace linux_host
    {
        // ------------------------------------------------------------------
        // App-level host: one per process, set up by runApp().
        // ------------------------------------------------------------------

        /// Creates the wake-up eventfd. `device` is the GPU device every
        /// window shares (created with no window: createDefaultDevice(nullptr)).
        bool init(std::shared_ptr<campello_gpu::Device> device);
        /// Runs any tasks still queued and closes the eventfd.
        void shutdown();
        std::shared_ptr<campello_gpu::Device> device();

        /// Runs `task` on the UI thread on a later turn of the event loop.
        /// Safe to call from any thread.
        void postTask(std::function<void()> task);
        /// Runs every queued task (the loop calls it each turn; teardown too).
        void runTasks();

        /// Readable when the loop should wake up (a task was posted or a
        /// window asked for a frame). Add it to the loop's poll() set.
        int  wakeFd();
        /// Makes wakeFd() readable. Safe to call from any thread.
        void wake();
        /// Empties wakeFd() (call after poll() reports it readable).
        void clearWake();

        // ------------------------------------------------------------------
        // One native window: the window-system-independent half.
        // ------------------------------------------------------------------

        class LinuxWindow
        {
        public:
            /// Installs `view`'s frame-request hook. `view` must outlive the window.
            explicit LinuxWindow(View& view);
            virtual ~LinuxWindow();

            LinuxWindow(const LinuxWindow&)            = delete;
            LinuxWindow& operator=(const LinuxWindow&) = delete;

            View& view() noexcept { return view_; }

            /// Creates the window's campello_gpu Surface on the shared device.
            /// Call once the native window exists. False if it can't present.
            bool attachSurface(const campello_gpu::LinuxSurfaceInfo& info);
            bool hasSurface() const noexcept { return surface_ != nullptr; }

            /// (Re)creates the renderer and raster thread for `root` (nullptr:
            /// no content), and makes it the View's hit-test root.
            void      setRoot(std::shared_ptr<RenderBox> root, Color background = Color::white());
            Renderer* renderer() const noexcept { return renderer_.get(); }

            /// Asks for a frame on the next loop turn.
            void requestFrame();
            bool needsFrame() const noexcept { return needs_frame_; }
            /// Builds this window's frame (if one was requested, the window is
            /// shown and its frame interval has passed) and hands it to its
            /// raster thread. True if submitted.
            bool drawFrame();

            /// Paces frames: at most one per `interval` (e.g. the display's
            /// refresh period). Zero (the default): no pacing, for windows the
            /// compositor paces (Wayland frame callbacks).
            void setMinFrameInterval(std::chrono::nanoseconds interval) noexcept { min_interval_ = interval; }

            /// The backing size changed (physical pixels): resizes the
            /// swapchain. Call notifyMetricsChanged() afterwards.
            void resizeSurface(uint32_t width, uint32_t height);
            /// Size or pixel ratio changed: repaints everything and calls
            /// on_metrics_changed.
            void notifyMetricsChanged();

            /// Backing size in physical pixels.
            virtual uint32_t physicalWidth() const  = 0;
            virtual uint32_t physicalHeight() const = 0;
            /// Physical pixels per logical pixel.
            virtual float    devicePixelRatio() const = 0;
            /// False while the window can't be seen (unmapped, minimised, not
            /// yet configured). Hidden windows aren't drawn: with FIFO
            /// presentation a hidden window's presents can block, and its
            /// raster thread would then stall every other window's (campello_gpu
            /// serialises recording per device).
            virtual bool     isShown() const = 0;

            Size logicalSize() const;

            /// The user asked to close the window (close button, WM_DELETE_WINDOW).
            std::function<void()> on_close;
            /// Size or device pixel ratio changed.
            std::function<void()> on_metrics_changed;

            /// Live windows, in creation order.
            static const std::vector<LinuxWindow*>& all();
            static bool         isLive(const LinuxWindow* window);
            static LinuxWindow* forView(const View* view);
            /// True if any live, shown window has a frame pending.
            static bool         anyFramePending();
            /// drawFrame() on every live window (a frame may destroy windows).
            static void         drawPendingFrames();
            /// Milliseconds until some shown window's requested frame is due
            /// (0: now), or -1 if none is pending. For the loop's poll().
            static int          msUntilNextFrame();

        private:
            void stopRendering();

            View&                                  view_;
            std::shared_ptr<campello_gpu::Surface> surface_;
            std::shared_ptr<RenderBox>             root_;
            std::shared_ptr<Renderer>              renderer_;
            std::unique_ptr<RasterThread>          raster_;  // stopped before renderer_/surface_ go
            bool                                   needs_frame_ = true;
            std::chrono::nanoseconds               min_interval_{0};
            std::chrono::steady_clock::time_point  last_frame_{};
        };

    } // namespace linux_host
} // namespace systems::leal::campello_widgets
