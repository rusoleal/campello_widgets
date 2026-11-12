// Web platform layer: a <canvas> with a WebGPU surface, the browser's
// requestAnimationFrame loop and DOM input events (per canvas: web_canvas.cpp;
// more canvases as windows: platform_windowing.cpp). Rendering is the shared
// campello_gpu backend (src/gpu/vulkan/) with WGSL shaders.

#include <campello_widgets/web/run_app.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/adapter.hpp>
#include <campello_gpu/texture_view.hpp>

#include "web_canvas.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

extern "C" {
void cw_web_set_title(const char* title);
int  cw_web_prefers_dark();
}

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

namespace systems::leal::campello_widgets {
void initializeWebPlatformMenuDelegate();            // platform_menu_delegate.cpp
std::unique_ptr<PlatformWindowing> makeWebPlatformWindowing(  // platform_windowing.cpp
    std::shared_ptr<campello_gpu::Device> device);
}

namespace {

struct WebAppState
{
    // The runApp() canvas's View (Phase 17). Declared first so it is destroyed last.
    std::unique_ptr<Widgets::View>              view;
    std::shared_ptr<GPU::Device>                device;
    std::unique_ptr<Widgets::WebCanvas>         canvas;
    std::unique_ptr<Widgets::PlatformWindowing> windowing;
    std::shared_ptr<Widgets::Element>           root_element;
    std::unique_ptr<Widgets::TickerScheduler>   ticker_scheduler;

    Widgets::MediaQueryData media_data;
    Widgets::WidgetRef      user_root_widget;
};

WebAppState* gState = nullptr;

// The canvas's CSS size or the page's devicePixelRatio changed.
void onMainCanvasMetrics(WebAppState* state)
{
    const float dpr = state->canvas->devicePixelRatio();
    Widgets::MediaQueryData data = state->media_data;
    data.device_pixel_ratio = dpr;
    data.logical_size = Widgets::Size{ state->canvas->width() / dpr, state->canvas->height() / dpr };
    if (!(data != state->media_data) || !state->root_element) return;
    state->media_data = data;
    Widgets::View::Scope scope(state->view.get());
    state->root_element->update(
        std::make_shared<Widgets::MediaQuery>(state->media_data, state->user_root_widget));
    Widgets::FrameScheduler::scheduleFrame();
}

} // namespace

namespace systems::leal::campello_widgets
{

int runApp(const std::string& title, int /*width*/, int /*height*/, WidgetRef root_widget)
{
    return runApp(title, std::move(root_widget), "#canvas");
}

int runApp(const std::string& title, WidgetRef root_widget, const std::string& canvas_selector)
{
    cw_web_set_title(title.c_str());

    // Lives for the rest of the page: the browser drives the app from here.
    auto* state = new WebAppState();
    gState = state;

    auto adapters = GPU::Device::getAdapters();
    if (adapters.empty()) {
        std::fprintf(stderr, "[Web] WebGPU is not available in this browser\n");
        return 1;
    }
    // No canvas of its own: every canvas (this one, and any opened through
    // WindowController) gets a Surface on this device.
    state->device = GPU::Device::createDevice(adapters[0], nullptr);
    if (!state->device) {
        std::fprintf(stderr, "[Web] Failed to create the WebGPU device\n");
        return 1;
    }

    // This canvas's View owns its dispatcher, focus and text input managers
    // and frame hook; makePrimary() also installs them as the process-wide
    // fallbacks, so code without a window context behaves as before.
    state->view = std::make_unique<View>();
    state->view->makePrimary();
    state->canvas = std::make_unique<WebCanvas>(state->device, canvas_selector, *state->view);
    if (!state->canvas->ok()) {
        std::fprintf(stderr, "[Web] No usable canvas %s\n", canvas_selector.c_str());
        return 1;
    }
    state->canvas->on_metrics_changed = [state] { onMainCanvasMetrics(state); };

    state->ticker_scheduler = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(state->ticker_scheduler.get());

    // Multi-window: WindowController opens more canvases on this device.
    state->windowing = makeWebPlatformWindowing(state->device);
    PlatformWindowing::setInstance(state->windowing.get());

    const float dpr = state->canvas->devicePixelRatio();
    MediaQueryData media;
    media.device_pixel_ratio  = dpr;
    media.platform_brightness = cw_web_prefers_dark() ? Brightness::dark : Brightness::light;
    media.logical_size        = Size{ state->canvas->width() / dpr, state->canvas->height() / dpr };
    state->media_data       = media;
    state->user_root_widget = std::move(root_widget);

    ThreadChecker::instance().bindToCurrentThread();
    initializeWebPlatformMenuDelegate();

    auto wrapped = std::make_shared<MediaQuery>(media, state->user_root_widget);
    state->root_element = wrapped->createElement();
    {
        View::Scope view_scope(state->view.get());
        state->root_element->mount(nullptr);
    }

    auto* roe = state->root_element->findDescendantRenderObjectElement();
    if (!roe) {
        std::fprintf(stderr, "[Web] Widget tree produced no RenderObjectElement\n");
        return 1;
    }
    auto render_box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
    if (!render_box) {
        std::fprintf(stderr, "[Web] Root render object is not a RenderBox\n");
        return 1;
    }
    state->canvas->setRoot(render_box);

    WebCanvas::installGlobalHandlers();
    return 0;
}

} // namespace systems::leal::campello_widgets
