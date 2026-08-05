// Headless embedding entry point — see inc/campello_widgets/linux/embedded_app.hpp
// for the full contract. Reuses the exact mount/dispatcher/focus-manager
// setup run_app.cpp/wayland_runner.cpp already establish for a real window,
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

    dispatcher_ = std::make_shared<PointerDispatcher>();
    PointerDispatcher::setActiveDispatcher(dispatcher_.get());

    focus_manager_ = std::make_shared<FocusManager>();
    FocusManager::setActiveManager(focus_manager_.get());

    text_input_manager_ = std::make_unique<TextInputManager>();
    TextInputManager::setActiveManager(text_input_manager_.get());

    ticker_scheduler_ = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(ticker_scheduler_.get());

    if (on_redraw_needed) {
        FrameScheduler::setCallback(std::move(on_redraw_needed));
    }

    MediaQueryData media_data;
    media_data.device_pixel_ratio = 1.0f;
    media_data.logical_size       = Size{width, height};

    auto wrapped_root = std::make_shared<MediaQuery>(media_data, std::move(root_widget));

    // Bind the UI thread before any widget tree mutation — same requirement
    // every other platform entry point has.
    ThreadChecker::instance().bindToCurrentThread();

    root_element_ = wrapped_root->createElement();
    root_element_->mount(nullptr);

    auto* roe = root_element_->findDescendantRenderObjectElement();
    if (!roe) {
        throw std::runtime_error("EmbeddedApp: widget tree produced no RenderObjectElement");
    }
    auto render_box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
    if (!render_box) {
        throw std::runtime_error("EmbeddedApp: root render object is not a RenderBox");
    }
    dispatcher_->setRoot(render_box);

    auto backend = std::make_unique<VulkanDrawBackend>(device_, background, pixel_format);
    renderer_ = std::make_shared<Renderer>(device_, render_box, background);
    renderer_->setDrawBackend(std::move(backend));
}

EmbeddedApp::~EmbeddedApp()
{
    // Same ordering rationale as run_app.cpp/wayland_runner.cpp's cleanup:
    // stop scheduling first, unmount the tree while the managers it may
    // still call into (activeDispatcher()/activeManager()) are alive, then
    // release the renderer/device.
    //
    // Shared limitation with every other platform entry point: these are
    // process-global singletons, so this unconditionally clears whichever
    // instance is currently active. If the host ever constructs a second
    // EmbeddedApp (or any windowed runApp()) before destroying this one,
    // that instance's own setActive*() calls already overwrote these
    // pointers — this destructor would then incorrectly null out the
    // *other* instance's active state. Not a new gap introduced here: no
    // platform in this library supports more than one concurrently-active
    // tree per process today.
    FrameScheduler::setCallback({});

    PointerDispatcher::setActiveDispatcher(nullptr);
    FocusManager::setActiveManager(nullptr);
    TextInputManager::setActiveManager(nullptr);
    TickerScheduler::setActive(nullptr);

    root_element_.reset();
    renderer_.reset();
    device_.reset();
}

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
    dispatcher_->tick(now_ms);
    ticker_scheduler_->tick(now_ms);
}

} // namespace systems::leal::campello_widgets
