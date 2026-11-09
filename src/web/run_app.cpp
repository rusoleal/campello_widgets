// Web platform layer: a <canvas> with a WebGPU surface, the browser's
// requestAnimationFrame loop and DOM input events. Rendering is the shared
// campello_gpu backend (src/gpu/vulkan/) with WGSL shaders.

#include <campello_widgets/web/run_app.hpp>
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
#include <campello_widgets/ui/system_mouse_cursor.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/adapter.hpp>
#include <campello_gpu/texture_view.hpp>

#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

extern "C" {
int  cw_web_sync_canvas(const char* selector, int* out_size, float* out_dpr, float* out_origin);
void cw_web_set_title(const char* title);
void cw_web_set_cursor(const char* selector, const char* cursor);
int  cw_web_prefers_dark();
void cw_web_install_paste_listener();
void cw_web_start_frame_loop();
}

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

namespace systems::leal::campello_widgets {
void webClipboardSetLocal(const std::string& text);  // clipboard.cpp
void initializeWebPlatformMenuDelegate();            // platform_menu_delegate.cpp
}

namespace {

struct WebAppState
{
    std::string                                 canvas_selector;
    std::shared_ptr<GPU::Device>                device;
    std::shared_ptr<Widgets::Renderer>          renderer;
    std::shared_ptr<Widgets::Element>           root_element;
    std::shared_ptr<Widgets::RenderBox>         render_box;
    std::shared_ptr<Widgets::PointerDispatcher> dispatcher;
    std::shared_ptr<Widgets::FocusManager>      focus_manager;
    std::unique_ptr<Widgets::TickerScheduler>   ticker_scheduler;
    std::unique_ptr<Widgets::TextInputManager>  text_input_manager;

    Widgets::MediaQueryData media_data;
    Widgets::WidgetRef      user_root_widget;

    int   width  = 1;   // canvas backing store, physical pixels
    int   height = 1;
    float dpr    = 1.0f;
    float origin_x = 0.0f;  // canvas position in the viewport, CSS pixels
    float origin_y = 0.0f;
    bool  needs_redraw   = true;
    bool  mouse_pressed  = false;
    uint32_t pending_paste_modifiers = 0;  // of a held-back Ctrl/Cmd+V
};

WebAppState* gState = nullptr;

uint64_t nowMs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void rebuildMediaQuery(WebAppState* state)
{
    if (!state->root_element) return;
    auto query = std::make_shared<Widgets::MediaQuery>(state->media_data, state->user_root_widget);
    state->root_element->update(query);
    Widgets::FrameScheduler::scheduleFrame();
}

// Follows the canvas's CSS size and the page's devicePixelRatio.
void syncCanvasSize(WebAppState* state)
{
    int size[2] = { state->width, state->height };
    float dpr = state->dpr;
    float origin[2] = {};
    cw_web_sync_canvas(state->canvas_selector.c_str(), size, &dpr, origin);
    state->origin_x = origin[0];
    state->origin_y = origin[1];
    if (size[0] == state->width && size[1] == state->height && dpr == state->dpr) return;
    state->width  = size[0];
    state->height = size[1];
    state->dpr    = dpr;
    if (state->renderer) state->renderer->setDevicePixelRatio(dpr);
    Widgets::MediaQueryData data = state->media_data;
    data.device_pixel_ratio = dpr;
    data.logical_size = Widgets::Size{ size[0] / dpr, size[1] / dpr };
    if (data != state->media_data) {
        state->media_data = data;
        rebuildMediaQuery(state);
    }
    state->needs_redraw = true;
}

void renderFrame(WebAppState* state)
{
    auto package = state->renderer->buildFrame(static_cast<float>(state->width),
                                               static_cast<float>(state->height));
    if (!package) return;
    package->target = state->device->getSwapchainTextureView();
    if (!package->target) return;
    // No raster thread on the web: the frame is encoded and submitted here,
    // and the browser presents the canvas when this task returns.
    state->renderer->rasterFrame(*package);
}

void onAnimationFrame(WebAppState* state)
{
    syncCanvasSize(state);
    if (state->needs_redraw) {
        state->needs_redraw = false;
        renderFrame(state);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

Widgets::PointerButton domButton(unsigned short button)
{
    switch (button) {
        case 0: return Widgets::PointerButton::primary;
        case 1: return Widgets::PointerButton::tertiary;
        case 2: return Widgets::PointerButton::secondary;
        default: return Widgets::PointerButton::unknown;
    }
}

EM_BOOL onMouse(int type, const EmscriptenMouseEvent* ev, void* user)
{
    auto* state = static_cast<WebAppState*>(user);
    if (!state->dispatcher) return EM_FALSE;

    Widgets::PointerEvent e;
    e.pointer_id   = 0;
    e.device_kind  = Widgets::PointerDeviceKind::mouse;
    // Canvas-relative from client coordinates: moves/releases are listened
    // for on the window, where targetX/Y aren't canvas-relative.
    e.position     = { static_cast<float>(ev->clientX) - state->origin_x,
                       static_cast<float>(ev->clientY) - state->origin_y };
    e.timestamp_ms = nowMs();
    switch (type) {
        case EMSCRIPTEN_EVENT_MOUSEDOWN:
            e.kind = Widgets::PointerEventKind::down;
            e.pressure = 1.0f;
            e.button = domButton(ev->button);
            state->mouse_pressed = true;
            break;
        case EMSCRIPTEN_EVENT_MOUSEUP:
            if (!state->mouse_pressed) return EM_FALSE;
            e.kind = Widgets::PointerEventKind::up;
            e.pressure = 0.0f;
            e.button = domButton(ev->button);
            state->mouse_pressed = false;
            break;
        default:
            e.kind = Widgets::PointerEventKind::move;
            e.pressure = state->mouse_pressed ? 1.0f : 0.0f;
            break;
    }
    state->dispatcher->handlePointerEvent(e);
    return type == EMSCRIPTEN_EVENT_MOUSEDOWN ? EM_TRUE : EM_FALSE;
}

EM_BOOL onWheel(int /*type*/, const EmscriptenWheelEvent* ev, void* user)
{
    auto* state = static_cast<WebAppState*>(user);
    if (!state->dispatcher) return EM_FALSE;
    // DOM deltas: positive = scroll down/right, the framework's convention.
    float scale = 1.0f;
    if (ev->deltaMode == DOM_DELTA_LINE) scale = 16.0f;
    else if (ev->deltaMode == DOM_DELTA_PAGE) scale = static_cast<float>(state->height) / state->dpr;

    Widgets::PointerEvent e;
    e.kind           = Widgets::PointerEventKind::scroll;
    e.pointer_id     = 0;
    e.device_kind    = Widgets::PointerDeviceKind::mouse;
    e.position       = { static_cast<float>(ev->mouse.clientX) - state->origin_x,
                         static_cast<float>(ev->mouse.clientY) - state->origin_y };
    e.scroll_delta_x = static_cast<float>(ev->deltaX) * scale;
    e.scroll_delta_y = static_cast<float>(ev->deltaY) * scale;
    e.timestamp_ms   = nowMs();
    state->dispatcher->handlePointerEvent(e);
    return EM_TRUE;  // don't scroll the page
}

Widgets::KeyCode domCodeToKeyCode(const char* code)
{
    using K = Widgets::KeyCode;
    const std::string c(code);
    if (c.size() == 4 && c.compare(0, 3, "Key") == 0 && c[3] >= 'A' && c[3] <= 'Z') {
        return static_cast<K>(static_cast<uint32_t>(K::a) + (c[3] - 'A'));
    }
    if (c.size() == 6 && c.compare(0, 5, "Digit") == 0 && c[5] >= '0' && c[5] <= '9') {
        return static_cast<K>(static_cast<uint32_t>(K::digit_0) + (c[5] - '0'));
    }
    if (c.size() >= 2 && c[0] == 'F' && c[1] >= '1' && c[1] <= '9') {
        int n = std::atoi(c.c_str() + 1);
        if (n >= 1 && n <= 12) return static_cast<K>(static_cast<uint32_t>(K::f1) + (n - 1));
    }
    struct Entry { const char* code; K key; };
    static const Entry kTable[] = {
        {"Space", K::space}, {"Enter", K::enter}, {"NumpadEnter", K::enter}, {"Tab", K::tab},
        {"Backspace", K::backspace}, {"Escape", K::escape}, {"Delete", K::delete_forward},
        {"ArrowLeft", K::left}, {"ArrowRight", K::right}, {"ArrowUp", K::up},
        {"ArrowDown", K::down}, {"Home", K::home}, {"End", K::end},
        {"PageUp", K::page_up}, {"PageDown", K::page_down},
        {"ShiftLeft", K::left_shift}, {"ShiftRight", K::right_shift},
        {"ControlLeft", K::left_ctrl}, {"ControlRight", K::right_ctrl},
        {"AltLeft", K::left_alt}, {"AltRight", K::right_alt},
        {"MetaLeft", K::left_meta}, {"MetaRight", K::right_meta}, {"CapsLock", K::caps_lock},
    };
    for (const auto& e : kTable) {
        if (c == e.code) return e.key;
    }
    return K::unknown;
}

// The code point of a DOM `key` value that is a single printable character
// ("a", "€"); 0 for named keys ("Enter", "ArrowLeft").
uint32_t printableCharacter(const char* key)
{
    const auto* s = reinterpret_cast<const unsigned char*>(key);
    uint32_t cp = 0;
    int len = 0;
    if (s[0] == 0) return 0;
    if (s[0] < 0x80) { cp = s[0]; len = 1; }
    else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; len = 2; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; len = 3; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; len = 4; }
    else return 0;
    for (int i = 1; i < len; ++i) {
        if ((s[i] & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (s[len] != 0) return 0;  // more than one character: a named key
    return cp >= 0x20 && cp != 0x7F ? cp : 0;
}

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* ev, void* user)
{
    auto* state = static_cast<WebAppState*>(user);
    if (!state->focus_manager) return EM_FALSE;

    uint32_t mods = Widgets::KeyModifiers::none;
    if (ev->shiftKey) mods |= Widgets::KeyModifiers::shift;
    if (ev->ctrlKey)  mods |= Widgets::KeyModifiers::ctrl;
    if (ev->altKey)   mods |= Widgets::KeyModifiers::alt;
    if (ev->metaKey)  mods |= Widgets::KeyModifiers::meta;

    Widgets::KeyEvent ke;
    ke.key_code  = domCodeToKeyCode(ev->code);
    ke.modifiers = mods;
    // Paste: let the browser fire its paste event (only it can read the
    // system clipboard); cw_web_on_paste() then delivers this shortcut.
    if (type == EMSCRIPTEN_EVENT_KEYDOWN && ke.key_code == Widgets::KeyCode::v &&
        (ev->ctrlKey || ev->metaKey)) {
        state->pending_paste_modifiers = mods;
        return EM_FALSE;
    }
    if (type == EMSCRIPTEN_EVENT_KEYUP) {
        ke.kind = Widgets::KeyEventKind::up;
        ke.character = 0;
    } else {
        ke.kind = ev->repeat ? Widgets::KeyEventKind::repeat : Widgets::KeyEventKind::down;
        ke.character = (ev->ctrlKey || ev->metaKey) ? 0 : printableCharacter(ev->key);
    }
    state->focus_manager->handleKeyEvent(ke);
    // Keep browser shortcuts (reload, devtools, copy/paste) working;
    // swallow the rest so Tab/Backspace/Space don't act on the page.
    if (ev->ctrlKey || ev->metaKey) return EM_FALSE;
    if (ke.key_code >= Widgets::KeyCode::f1 && ke.key_code <= Widgets::KeyCode::f12) return EM_FALSE;
    return EM_TRUE;
}

const char* cssCursor(Widgets::SystemMouseCursor cursor)
{
    switch (cursor) {
        case Widgets::SystemMouseCursor::pointer:   return "pointer";
        case Widgets::SystemMouseCursor::text:      return "text";
        case Widgets::SystemMouseCursor::forbidden: return "not-allowed";
        case Widgets::SystemMouseCursor::resize_ns: return "ns-resize";
        case Widgets::SystemMouseCursor::resize_ew: return "ew-resize";
        default:                                    return "default";
    }
}

} // namespace

// One requestAnimationFrame tick (the loop is cw_web_start_frame_loop()).
extern "C" EMSCRIPTEN_KEEPALIVE void cw_web_frame()
{
    if (gState) onAnimationFrame(gState);
}

// The page's paste listener (src/web/campello_widgets_web.js): stores the
// text for Clipboard::getText(), then delivers the held-back shortcut.
extern "C" EMSCRIPTEN_KEEPALIVE void cw_web_on_paste(const char* text)
{
    Widgets::webClipboardSetLocal(text ? text : "");
    WebAppState* state = gState;
    if (!state || !state->focus_manager || state->pending_paste_modifiers == 0) return;
    Widgets::KeyEvent ke;
    ke.kind      = Widgets::KeyEventKind::down;
    ke.key_code  = Widgets::KeyCode::v;
    ke.modifiers = state->pending_paste_modifiers;
    ke.character = 0;
    state->pending_paste_modifiers = 0;
    state->focus_manager->handleKeyEvent(ke);
}

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
    state->canvas_selector = canvas_selector;

    int size[2] = { 1, 1 };
    float dpr = 1.0f;
    float origin[2] = {};
    cw_web_sync_canvas(canvas_selector.c_str(), size, &dpr, origin);
    state->origin_x = origin[0];
    state->origin_y = origin[1];
    state->width  = size[0];
    state->height = size[1];
    state->dpr    = dpr;

    auto adapters = GPU::Device::getAdapters();
    if (adapters.empty()) {
        std::fprintf(stderr, "[Web] WebGPU is not available in this browser\n");
        return 1;
    }
    state->device = GPU::Device::createDevice(adapters[0],
                                              const_cast<char*>(canvas_selector.c_str()));
    if (!state->device) {
        std::fprintf(stderr, "[Web] Failed to create the WebGPU device for %s\n",
                     canvas_selector.c_str());
        return 1;
    }

    state->dispatcher = std::make_shared<PointerDispatcher>();
    PointerDispatcher::setActiveDispatcher(state->dispatcher.get());
    state->focus_manager = std::make_shared<FocusManager>();
    FocusManager::setActiveManager(state->focus_manager.get());
    state->text_input_manager = std::make_unique<TextInputManager>();
    TextInputManager::setActiveManager(state->text_input_manager.get());
    state->ticker_scheduler = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(state->ticker_scheduler.get());

    FrameScheduler::setCallback([state]() { state->needs_redraw = true; });
    registerCursorHandler([state](SystemMouseCursor cursor) {
        cw_web_set_cursor(state->canvas_selector.c_str(), cssCursor(cursor));
    });

    MediaQueryData media;
    media.device_pixel_ratio  = dpr;
    media.platform_brightness = cw_web_prefers_dark() ? Brightness::dark : Brightness::light;
    media.logical_size        = Size{ size[0] / dpr, size[1] / dpr };
    state->media_data       = media;
    state->user_root_widget = std::move(root_widget);

    ThreadChecker::instance().bindToCurrentThread();
    initializeWebPlatformMenuDelegate();

    auto wrapped = std::make_shared<MediaQuery>(media, state->user_root_widget);
    state->root_element = wrapped->createElement();
    state->root_element->mount(nullptr);

    auto* roe = state->root_element->findDescendantRenderObjectElement();
    if (!roe) {
        std::fprintf(stderr, "[Web] Widget tree produced no RenderObjectElement\n");
        return 1;
    }
    state->render_box = std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject());
    if (!state->render_box) {
        std::fprintf(stderr, "[Web] Root render object is not a RenderBox\n");
        return 1;
    }
    state->dispatcher->setRoot(state->render_box);

    const Color background = Color::white();
    auto backend = std::make_unique<VulkanDrawBackend>(
        state->device, background, state->device->getSwapchainPixelFormat());
    state->renderer = std::make_shared<Renderer>(state->device, state->render_box, background);
    state->renderer->setDrawBackend(std::move(backend));
    state->renderer->setDevicePixelRatio(dpr);

    const char* target = state->canvas_selector.c_str();
    emscripten_set_mousedown_callback(target, state, EM_TRUE, onMouse);
    // Moves and releases are tracked on the window so drags keep working
    // outside the canvas; positions stay canvas-relative (targetX/Y).
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, EM_TRUE, onMouse);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, EM_TRUE, onMouse);
    emscripten_set_wheel_callback(target, state, EM_TRUE, onWheel);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, EM_TRUE, onKey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, state, EM_TRUE, onKey);

    cw_web_install_paste_listener();

    state->needs_redraw = true;
    cw_web_start_frame_loop();
    return 0;
}

} // namespace systems::leal::campello_widgets
