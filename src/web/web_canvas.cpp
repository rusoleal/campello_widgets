#include "web_canvas.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/surface.hpp>

#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int  cw_web_sync_canvas(const char* selector, int* out_size, float* out_dpr, float* out_origin);
int  cw_web_is_topmost_at(const char* selector, float client_x, float client_y);
void cw_web_set_cursor(const char* selector, const char* cursor);
void cw_web_install_paste_listener();
void cw_web_start_frame_loop();
}

namespace GPU = ::systems::leal::campello_gpu;

namespace systems::leal::campello_widgets
{
    void webClipboardSetLocal(const std::string& text);  // clipboard.cpp

    namespace
    {
        // Every live canvas, in creation order (the runApp() one first).
        std::vector<WebCanvas*>& canvasRegistry()
        {
            static std::vector<WebCanvas*> r;
            return r;
        }

        // Renderers no longer drawn, released at the start of the next frame
        // (see WebCanvas::setRoot()).
        std::vector<std::shared_ptr<Renderer>>& retiredRenderers()
        {
            static std::vector<std::shared_ptr<Renderer>> retired;
            return retired;
        }

        WebCanvas* g_key      = nullptr;  // receives keys
        WebCanvas* g_pressed  = nullptr;  // has the mouse button down: gets moves/release
        WebCanvas* g_hovered  = nullptr;  // under the pointer (when no button is down)
        uint32_t   g_pending_paste_modifiers = 0;  // of a held-back Ctrl/Cmd+V

        bool isLive(WebCanvas* c)
        {
            auto& r = canvasRegistry();
            return c && std::find(r.begin(), r.end(), c) != r.end();
        }

        uint64_t nowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        }

        PointerButton domButton(unsigned short button)
        {
            switch (button) {
                case 0: return PointerButton::primary;
                case 1: return PointerButton::tertiary;
                case 2: return PointerButton::secondary;
                default: return PointerButton::unknown;
            }
        }

        KeyCode domCodeToKeyCode(const char* code)
        {
            using K = KeyCode;
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

        // The code point of a DOM `key` value that is a single printable
        // character ("a", "€"); 0 for named keys ("Enter", "ArrowLeft").
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

        const char* cssCursor(SystemMouseCursor cursor)
        {
            switch (cursor) {
                case SystemMouseCursor::pointer:   return "pointer";
                case SystemMouseCursor::text:      return "text";
                case SystemMouseCursor::forbidden: return "not-allowed";
                case SystemMouseCursor::resize_ns: return "ns-resize";
                case SystemMouseCursor::resize_ew: return "ew-resize";
                default:                           return "default";
            }
        }
    } // namespace

    // DOM callbacks; a struct so they can reach WebCanvas's private state.
    struct WebCanvas::Input
    {
        static void deliver(WebCanvas* c, PointerEvent e)
        {
            View::Scope scope(&c->view_);
            c->view_.pointerDispatcher().handlePointerEvent(e);
        }

        static PointerEvent mouseEvent(WebCanvas* c, const EmscriptenMouseEvent* ev)
        {
            PointerEvent e;
            e.pointer_id   = 0;
            e.device_kind  = PointerDeviceKind::mouse;
            // Canvas-relative from client coordinates: moves/releases are
            // listened for on the window, where targetX/Y aren't canvas-relative.
            e.position     = { static_cast<float>(ev->clientX) - c->origin_x_,
                               static_cast<float>(ev->clientY) - c->origin_y_ };
            e.timestamp_ms = nowMs();
            return e;
        }

        // A move far outside every canvas: hover exits in `c`'s tree (the
        // pointer may still be inside its bounds, over a canvas above it).
        static void leave(WebCanvas* c)
        {
            PointerEvent e;
            e.kind         = PointerEventKind::move;
            e.pointer_id   = 0;
            e.device_kind  = PointerDeviceKind::mouse;
            e.position     = { -1.0e6f, -1.0e6f };
            e.timestamp_ms = nowMs();
            deliver(c, e);
        }

        // mousedown, on each canvas.
        static EM_BOOL onDown(int, const EmscriptenMouseEvent* ev, void* user)
        {
            auto* c = static_cast<WebCanvas*>(user);
            if (!isLive(c)) return EM_FALSE;

            // Popups close when the user presses anywhere else.
            const std::vector<WebCanvas*> canvases = canvasRegistry();
            for (WebCanvas* other : canvases)
                if (other != c && isLive(other) && other->is_popup && other->on_outside_press)
                {
                    auto callback = other->on_outside_press;
                    callback();
                }

            // A modal window blocks this canvas: focus the modal instead
            // (the View drops the input anyway).
            if (c->view_.isInputBlocked())
            {
                if (WebCanvas* modal = forView(c->view_.inputBlocker())) modal->makeKey();
                return EM_TRUE;
            }
            if (g_hovered && g_hovered != c && isLive(g_hovered)) leave(g_hovered);
            g_hovered = c;
            g_pressed = c;
            c->makeKey();
            PointerEvent e = mouseEvent(c, ev);
            e.kind     = PointerEventKind::down;
            e.pressure = 1.0f;
            e.button   = domButton(ev->button);
            deliver(c, e);
            return EM_TRUE;
        }

        // mousemove and mouseup, on the window (drags keep working outside
        // the canvas they started in).
        // `user`: the popup canvas whose window this came from, or nullptr
        // for the page's own window.
        static EM_BOOL onWindowMouse(int type, const EmscriptenMouseEvent* ev, void* user)
        {
            auto* popup = static_cast<WebCanvas*>(user);
            if (g_pressed && !isLive(g_pressed)) g_pressed = nullptr;
            if (g_hovered && !isLive(g_hovered)) g_hovered = nullptr;

            if (type == EMSCRIPTEN_EVENT_MOUSEUP) {
                WebCanvas* c = g_pressed;
                if (!c) return EM_FALSE;
                g_pressed = nullptr;
                PointerEvent e = mouseEvent(c, ev);
                e.kind     = PointerEventKind::up;
                e.pressure = 0.0f;
                e.button   = domButton(ev->button);
                deliver(c, e);
                return EM_FALSE;
            }

            if (g_pressed) {  // a drag: only the canvas it started in
                PointerEvent e = mouseEvent(g_pressed, ev);
                e.kind     = PointerEventKind::move;
                e.pressure = 1.0f;
                deliver(g_pressed, e);
                return EM_FALSE;
            }

            // Hover: the topmost canvas under the pointer in that window, if any.
            WebCanvas* over = nullptr;
            for (WebCanvas* c : canvasRegistry()) {
                if (popup ? c != popup : !c->host_window_.empty()) continue;
                if (cw_web_is_topmost_at(c->selector_.c_str(), (float)ev->clientX, (float)ev->clientY)) {
                    over = c;
                    break;
                }
            }
            if (g_hovered && g_hovered != over) leave(g_hovered);
            g_hovered = over;
            // With a single canvas, moves outside it still go to it (as
            // before Phase 17): its out-of-bounds position ends hovers.
            WebCanvas* target = over ? over : (canvasRegistry().size() == 1 ? canvasRegistry().front() : nullptr);
            if (popup && !over) target = popup;  // its only canvas: out-of-bounds ends hovers
            if (target) {
                PointerEvent e = mouseEvent(target, ev);
                e.kind     = PointerEventKind::move;
                e.pressure = 0.0f;
                deliver(target, e);
            }
            return EM_FALSE;
        }

        static EM_BOOL onWheel(int, const EmscriptenWheelEvent* ev, void* user)
        {
            auto* c = static_cast<WebCanvas*>(user);
            if (!isLive(c)) return EM_FALSE;
            // DOM deltas: positive = scroll down/right, the framework's convention.
            float scale = 1.0f;
            if (ev->deltaMode == DOM_DELTA_LINE) scale = 16.0f;
            else if (ev->deltaMode == DOM_DELTA_PAGE) scale = static_cast<float>(c->height_) / c->dpr_;

            PointerEvent e = mouseEvent(c, &ev->mouse);
            e.kind           = PointerEventKind::scroll;
            e.scroll_delta_x = static_cast<float>(ev->deltaX) * scale;
            e.scroll_delta_y = static_cast<float>(ev->deltaY) * scale;
            deliver(c, e);
            return EM_TRUE;  // don't scroll the page
        }

        static void deliverKey(WebCanvas* c, const KeyEvent& ke)
        {
            View::Scope scope(&c->view_);
            c->view_.focusManager().handleKeyEvent(ke);
        }

        static EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* ev, void* user)
        {
            // A popup's keys go to its canvas; the page's to its key canvas.
            WebCanvas* c = static_cast<WebCanvas*>(user);
            if (c) {
                if (!isLive(c)) return EM_FALSE;
                c->makeKey();
            } else {
                c = keyCanvas();
                if (c && !c->host_window_.empty()) {
                    c = nullptr;
                    for (WebCanvas* k : canvasRegistry())
                        if (k->host_window_.empty()) { c = k; break; }
                }
            }
            if (!c) return EM_FALSE;

            uint32_t mods = KeyModifiers::none;
            if (ev->shiftKey) mods |= KeyModifiers::shift;
            if (ev->ctrlKey)  mods |= KeyModifiers::ctrl;
            if (ev->altKey)   mods |= KeyModifiers::alt;
            if (ev->metaKey)  mods |= KeyModifiers::meta;

            KeyEvent ke;
            ke.key_code  = domCodeToKeyCode(ev->code);
            ke.modifiers = mods;
            // Paste: let the browser fire its paste event (only it can read
            // the system clipboard); cw_web_on_paste() then delivers this shortcut.
            if (type == EMSCRIPTEN_EVENT_KEYDOWN && ke.key_code == KeyCode::v &&
                (ev->ctrlKey || ev->metaKey)) {
                g_pending_paste_modifiers = mods;
                return EM_FALSE;
            }
            if (type == EMSCRIPTEN_EVENT_KEYUP) {
                ke.kind = KeyEventKind::up;
                ke.character = 0;
            } else {
                ke.kind = ev->repeat ? KeyEventKind::repeat : KeyEventKind::down;
                ke.character = (ev->ctrlKey || ev->metaKey) ? 0 : printableCharacter(ev->key);
            }
            deliverKey(c, ke);
            // Keep browser shortcuts (reload, devtools, copy/paste) working;
            // swallow the rest so Tab/Backspace/Space don't act on the page.
            if (ev->ctrlKey || ev->metaKey) return EM_FALSE;
            if (ke.key_code >= KeyCode::f1 && ke.key_code <= KeyCode::f12) return EM_FALSE;
            return EM_TRUE;
        }

        static void onPaste(const char* text)
        {
            webClipboardSetLocal(text ? text : "");
            WebCanvas* c = keyCanvas();
            if (!c || g_pending_paste_modifiers == 0) return;
            KeyEvent ke;
            ke.kind      = KeyEventKind::down;
            ke.key_code  = KeyCode::v;
            ke.modifiers = g_pending_paste_modifiers;
            ke.character = 0;
            g_pending_paste_modifiers = 0;
            deliverKey(c, ke);
        }
    };

    WebCanvas::WebCanvas(std::shared_ptr<GPU::Device> device, std::string selector, View& view,
                         std::string host_window)
        : device_(std::move(device)), selector_(std::move(selector)), host_window_(std::move(host_window))
        , view_(view)
    {
        float origin[2] = {};
        int size[2] = { 1, 1 };
        cw_web_sync_canvas(selector_.c_str(), size, &dpr_, origin);
        width_ = size[0]; height_ = size[1];
        origin_x_ = origin[0]; origin_y_ = origin[1];

        if (device_) surface_ = device_->createSurface(const_cast<char*>(selector_.c_str()));
        if (!surface_) {
            std::fprintf(stderr, "[Web] No WebGPU surface for canvas %s\n", selector_.c_str());
            return;
        }

        canvasRegistry().push_back(this);
        if (!g_key) g_key = this;

        view_.setFrameRequestCallback([this] { needs_redraw_ = true; });

        const char* target = selector_.c_str();
        emscripten_set_mousedown_callback(target, this, EM_TRUE, &Input::onDown);
        emscripten_set_wheel_callback(target, this, EM_TRUE, &Input::onWheel);
        if (!host_window_.empty()) {
            const char* win = host_window_.c_str();
            emscripten_set_mousemove_callback(win, this, EM_TRUE, &Input::onWindowMouse);
            emscripten_set_mouseup_callback(win, this, EM_TRUE, &Input::onWindowMouse);
            emscripten_set_keydown_callback(win, this, EM_TRUE, &Input::onKey);
            emscripten_set_keyup_callback(win, this, EM_TRUE, &Input::onKey);
        }
        listening_ = true;
    }

    WebCanvas::~WebCanvas()
    {
        if (listening_) {
            const char* target = selector_.c_str();
            emscripten_set_mousedown_callback(target, nullptr, EM_TRUE, nullptr);
            emscripten_set_wheel_callback(target, nullptr, EM_TRUE, nullptr);
            if (!host_window_.empty()) {
                const char* win = host_window_.c_str();
                emscripten_set_mousemove_callback(win, nullptr, EM_TRUE, nullptr);
                emscripten_set_mouseup_callback(win, nullptr, EM_TRUE, nullptr);
                emscripten_set_keydown_callback(win, nullptr, EM_TRUE, nullptr);
                emscripten_set_keyup_callback(win, nullptr, EM_TRUE, nullptr);
            }
        }
        auto& r = canvasRegistry();
        r.erase(std::remove(r.begin(), r.end(), this), r.end());
        if (g_key == this)     g_key = r.empty() ? nullptr : r.front();
        if (g_pressed == this) g_pressed = nullptr;
        if (g_hovered == this) g_hovered = nullptr;
        if (surface_) view_.setFrameRequestCallback(nullptr);
        setRoot(nullptr);
        surface_.reset();
    }

    void WebCanvas::setRoot(std::shared_ptr<RenderBox> root, Color background)
    {
        if (renderer_)
        {
            // Not destroyed here: a renderer's destructor waits for the GPU,
            // which on the web yields to the browser (ASYNCIFY), and other
            // tasks (e.g. this window's own deferred destroy) would then run
            // in the middle of whatever called us. Released at the start of
            // the next frame instead, where yielding is harmless.
            view_.setRenderer(nullptr);
            retiredRenderers().push_back(std::move(renderer_));
        }
        root_ = std::move(root);
        view_.pointerDispatcher().setRoot(root_);
        if (!root_ || !surface_) return;

        renderer_ = std::make_shared<Renderer>(device_, root_, background);
        renderer_->setDrawBackend(std::make_unique<VulkanDrawBackend>(
            device_, background, surface_->getPixelFormat()));
        renderer_->setDevicePixelRatio(dpr_);
        view_.setRenderer(renderer_.get());
        needs_redraw_ = true;
    }

    void WebCanvas::sync()
    {
        int size[2] = { width_, height_ };
        float dpr = dpr_;
        float origin[2] = { origin_x_, origin_y_ };
        cw_web_sync_canvas(selector_.c_str(), size, &dpr, origin);
        origin_x_ = origin[0];
        origin_y_ = origin[1];
        if (size[0] == width_ && size[1] == height_ && dpr == dpr_) return;
        width_  = size[0];
        height_ = size[1];
        dpr_    = dpr;
        if (renderer_) renderer_->setDevicePixelRatio(dpr);
        needs_redraw_ = true;
        auto callback = on_metrics_changed;  // may replace on_metrics_changed
        if (callback) callback();
    }

    void WebCanvas::frame()
    {
        if (on_frame) {
            auto callback = on_frame;
            if (!callback()) return;
        }
        sync();
        if (!needs_redraw_ || !renderer_) return;
        needs_redraw_ = false;
        // Held for the whole frame: building it runs every window's pending
        // rebuilds, which may replace or drop this canvas's renderer (e.g. a
        // popup closing itself), so renderer_ may change under us.
        const auto renderer = renderer_;
        const auto surface  = surface_;
        auto package = renderer->buildFrame(static_cast<float>(width_), static_cast<float>(height_));
        if (!package || renderer != renderer_) return;
        // No raster thread on the web: the frame is encoded and submitted
        // here, and the browser presents the canvas when this task returns.
        package->surface = surface;
        renderer->rasterFrame(*package);
    }

    void WebCanvas::makeKey() noexcept { g_key = this; }

    void WebCanvas::frameAll()
    {
        if (!retiredRenderers().empty())
        {
            auto retired = std::move(retiredRenderers());
            retiredRenderers().clear();
            retired.clear();  // may yield: nothing is in progress here
        }
        // A frame may open or close windows (a rebuild mounting a Window
        // widget): iterate over a snapshot, skipping canvases gone meanwhile.
        const std::vector<WebCanvas*> canvases = canvasRegistry();
        for (WebCanvas* c : canvases)
            if (isLive(c)) c->frame();
    }

    WebCanvas* WebCanvas::keyCanvas() noexcept { return isLive(g_key) ? g_key : nullptr; }

    WebCanvas* WebCanvas::forView(const View* view) noexcept
    {
        for (WebCanvas* c : canvasRegistry())
            if (&c->view_ == view) return c;
        return nullptr;
    }

    void WebCanvas::installGlobalHandlers()
    {
        emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, &Input::onWindowMouse);
        emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, &Input::onWindowMouse);
        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, &Input::onKey);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, &Input::onKey);
        cw_web_install_paste_listener();

        // A cursor change comes from the canvas whose events (or frame) are
        // being handled: its View is the ambient one.
        registerCursorHandler([](SystemMouseCursor cursor) {
            WebCanvas* c = forView(View::current());
            if (!c) c = keyCanvas();
            if (c) cw_web_set_cursor(c->selector_.c_str(), cssCursor(cursor));
        });

        cw_web_start_frame_loop();
    }

} // namespace systems::leal::campello_widgets

// One requestAnimationFrame tick (the loop is cw_web_start_frame_loop()).
extern "C" EMSCRIPTEN_KEEPALIVE void cw_web_frame()
{
    ::systems::leal::campello_widgets::WebCanvas::frameAll();
}

// The page's paste listener (src/web/campello_widgets_web.js): stores the
// text for Clipboard::getText(), then delivers the held-back shortcut.
extern "C" EMSCRIPTEN_KEEPALIVE void cw_web_on_paste(const char* text)
{
    ::systems::leal::campello_widgets::WebCanvas::Input::onPaste(text);
}
