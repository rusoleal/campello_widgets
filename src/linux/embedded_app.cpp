// Headless embedding entry point — see inc/campello_widgets/linux/embedded_app.hpp
// for the full contract. Gives the tree a View, as run_app.cpp and
// wayland_runner.cpp do for each window (Phase 17),
// minus everything window-specific: no X11/Wayland connection, no surface,
// no swapchain, no event loop, no IME (a host embedding this inside its own
// compositor is expected to own its own text-input story if it needs one;
// nothing here precludes wiring IBus back in later the same way the
// windowed runners do).

#include <campello_widgets/linux/embedded_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/view.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>

#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <stdexcept>

namespace GPU = ::systems::leal::campello_gpu;

namespace systems::leal::campello_widgets
{

EmbeddedApp::EmbeddedApp(
    std::shared_ptr<campello_gpu::Device> device,
    WidgetRef                             root_widget,
    float                                 width,
    float                                 height,
    Color                                 background,
    campello_gpu::PixelFormat             pixel_format,
    std::function<void()>                 on_redraw_needed)
    : device_(std::move(device))
{
    if (!device_) {
        throw std::invalid_argument("EmbeddedApp: device must not be null");
    }

    // Bind the UI thread before any widget tree mutation — same requirement
    // every other platform entry point has.
    ThreadChecker::instance().bindToCurrentThread();

    // The tree's View (Phase 17): its pointer dispatcher, focus and text
    // input. makePrimary() also installs them as the process-wide fallbacks.
    view_ = std::make_unique<View>();
    view_->makePrimary();
    // Frame requests from this tree (markNeedsPaint, setState, animations
    // through FrameScheduler) reach the host through the View.
    if (on_redraw_needed) view_->setFrameRequestCallback(std::move(on_redraw_needed));

    ticker_scheduler_ = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(ticker_scheduler_.get());

    MediaQueryData media_data;
    media_data.device_pixel_ratio = 1.0f;
    media_data.logical_size       = Size{width, height};

    auto wrapped_root = std::make_shared<MediaQuery>(media_data, std::move(root_widget));

    root_element_ = wrapped_root->createElement();
    {
        View::Scope scope(view_.get());
        root_element_->mount(nullptr);
    }

    auto* roe = root_element_->findDescendantRenderObjectElement();
    if (!roe) {
        throw std::runtime_error("EmbeddedApp: widget tree produced no RenderObjectElement");
    }
    auto render_box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
    if (!render_box) {
        throw std::runtime_error("EmbeddedApp: root render object is not a RenderBox");
    }
    view_->pointerDispatcher().setRoot(render_box);

    auto backend = std::make_unique<VulkanDrawBackend>(device_, background, pixel_format);
    renderer_ = std::make_shared<Renderer>(device_, render_box, background);
    renderer_->setDrawBackend(std::move(backend));
    view_->setRenderer(renderer_.get());  // also points the renderer at the View
}

EmbeddedApp::~EmbeddedApp()
{
    // Same order as the windowed runners: no more frame requests, the tree
    // unmounted inside its View while the managers it unregisters from still
    // exist, then the renderer (detached from the View), then the View.
    view_->setFrameRequestCallback(nullptr);
    {
        View::Scope scope(view_.get());
        if (root_element_) root_element_->unmount();
    }
    root_element_.reset();
    view_->pointerDispatcher().setRoot(nullptr);

    TickerScheduler::setActive(nullptr);
    ticker_scheduler_.reset();

    view_->setRenderer(nullptr);
    renderer_.reset();
    view_.reset();  // clears the process-wide fallbacks it installed
    device_.reset();
}

PointerDispatcher& EmbeddedApp::pointerDispatcher() noexcept { return view_->pointerDispatcher(); }
FocusManager&      EmbeddedApp::focusManager() noexcept      { return view_->focusManager(); }

bool EmbeddedApp::renderFrame(
    std::shared_ptr<campello_gpu::TextureView> target,
    float viewport_width,
    float viewport_height)
{
    auto package = renderer_->buildFrame(viewport_width, viewport_height);
    if (!package) return false;

    package->target = std::move(target);
    return renderer_->rasterFrame(*package);
}

void EmbeddedApp::forceRefresh()
{
    renderer_->forceRefresh();
}

void EmbeddedApp::tick(uint64_t now_ms)
{
    View::Scope scope(view_.get());
    view_->pointerDispatcher().tick(now_ms);
    ticker_scheduler_->tick(now_ms);
}

} // namespace systems::leal::campello_widgets
