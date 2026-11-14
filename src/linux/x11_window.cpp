#include "x11_window.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>

#include "ibus_ime.hpp"

#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Widgets = ::systems::leal::campello_widgets;
namespace GPU     = ::systems::leal::campello_gpu;

// ---------------------------------------------------------------------------
// X11 keycode translation (moved from run_app.cpp)
// ---------------------------------------------------------------------------

static Widgets::KeyCode x11KeysymToKeyCode(KeySym keysym)
{
    switch (keysym) {
        case XK_a: case XK_A: return Widgets::KeyCode::a;
        case XK_b: case XK_B: return Widgets::KeyCode::b;
        case XK_c: case XK_C: return Widgets::KeyCode::c;
        case XK_d: case XK_D: return Widgets::KeyCode::d;
        case XK_e: case XK_E: return Widgets::KeyCode::e;
        case XK_f: case XK_F: return Widgets::KeyCode::f;
        case XK_g: case XK_G: return Widgets::KeyCode::g;
        case XK_h: case XK_H: return Widgets::KeyCode::h;
        case XK_i: case XK_I: return Widgets::KeyCode::i;
        case XK_j: case XK_J: return Widgets::KeyCode::j;
        case XK_k: case XK_K: return Widgets::KeyCode::k;
        case XK_l: case XK_L: return Widgets::KeyCode::l;
        case XK_m: case XK_M: return Widgets::KeyCode::m;
        case XK_n: case XK_N: return Widgets::KeyCode::n;
        case XK_o: case XK_O: return Widgets::KeyCode::o;
        case XK_p: case XK_P: return Widgets::KeyCode::p;
        case XK_q: case XK_Q: return Widgets::KeyCode::q;
        case XK_r: case XK_R: return Widgets::KeyCode::r;
        case XK_s: case XK_S: return Widgets::KeyCode::s;
        case XK_t: case XK_T: return Widgets::KeyCode::t;
        case XK_u: case XK_U: return Widgets::KeyCode::u;
        case XK_v: case XK_V: return Widgets::KeyCode::v;
        case XK_w: case XK_W: return Widgets::KeyCode::w;
        case XK_x: case XK_X: return Widgets::KeyCode::x;
        case XK_y: case XK_Y: return Widgets::KeyCode::y;
        case XK_z: case XK_Z: return Widgets::KeyCode::z;
        case XK_0: return Widgets::KeyCode::digit_0;
        case XK_1: return Widgets::KeyCode::digit_1;
        case XK_2: return Widgets::KeyCode::digit_2;
        case XK_3: return Widgets::KeyCode::digit_3;
        case XK_4: return Widgets::KeyCode::digit_4;
        case XK_5: return Widgets::KeyCode::digit_5;
        case XK_6: return Widgets::KeyCode::digit_6;
        case XK_7: return Widgets::KeyCode::digit_7;
        case XK_8: return Widgets::KeyCode::digit_8;
        case XK_9: return Widgets::KeyCode::digit_9;
        case XK_space:      return Widgets::KeyCode::space;
        case XK_Return:     return Widgets::KeyCode::enter;
        case XK_Tab:        return Widgets::KeyCode::tab;
        case XK_BackSpace:  return Widgets::KeyCode::backspace;
        case XK_Escape:     return Widgets::KeyCode::escape;
        case XK_Delete:     return Widgets::KeyCode::delete_forward;
        case XK_Left:       return Widgets::KeyCode::left;
        case XK_Right:      return Widgets::KeyCode::right;
        case XK_Up:         return Widgets::KeyCode::up;
        case XK_Down:       return Widgets::KeyCode::down;
        case XK_Home:       return Widgets::KeyCode::home;
        case XK_End:        return Widgets::KeyCode::end;
        case XK_Page_Up:    return Widgets::KeyCode::page_up;
        case XK_Page_Down:  return Widgets::KeyCode::page_down;
        case XK_Shift_L:    return Widgets::KeyCode::left_shift;
        case XK_Shift_R:    return Widgets::KeyCode::right_shift;
        case XK_Control_L:  return Widgets::KeyCode::left_ctrl;
        case XK_Control_R:  return Widgets::KeyCode::right_ctrl;
        case XK_Alt_L:      return Widgets::KeyCode::left_alt;
        case XK_Alt_R:      return Widgets::KeyCode::right_alt;
        case XK_Super_L:    return Widgets::KeyCode::left_meta;
        case XK_Super_R:    return Widgets::KeyCode::right_meta;
        case XK_Caps_Lock:  return Widgets::KeyCode::caps_lock;
        case XK_F1:  return Widgets::KeyCode::f1;
        case XK_F2:  return Widgets::KeyCode::f2;
        case XK_F3:  return Widgets::KeyCode::f3;
        case XK_F4:  return Widgets::KeyCode::f4;
        case XK_F5:  return Widgets::KeyCode::f5;
        case XK_F6:  return Widgets::KeyCode::f6;
        case XK_F7:  return Widgets::KeyCode::f7;
        case XK_F8:  return Widgets::KeyCode::f8;
        case XK_F9:  return Widgets::KeyCode::f9;
        case XK_F10: return Widgets::KeyCode::f10;
        case XK_F11: return Widgets::KeyCode::f11;
        case XK_F12: return Widgets::KeyCode::f12;
        default:            return Widgets::KeyCode::unknown;
    }
}

static uint32_t x11StateToKeyModifiers(unsigned int state)
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (state & ShiftMask)   mods |= Widgets::KeyModifiers::shift;
    if (state & ControlMask) mods |= Widgets::KeyModifiers::ctrl;
    if (state & Mod1Mask)    mods |= Widgets::KeyModifiers::alt;
    if (state & Mod4Mask)    mods |= Widgets::KeyModifiers::meta;
    return mods;
}

static bool isNavigationOrSpecialKeyX11(Widgets::KeyCode key_code)
{
    return key_code == Widgets::KeyCode::left
        || key_code == Widgets::KeyCode::right
        || key_code == Widgets::KeyCode::up
        || key_code == Widgets::KeyCode::down
        || key_code == Widgets::KeyCode::home
        || key_code == Widgets::KeyCode::end
        || key_code == Widgets::KeyCode::page_up
        || key_code == Widgets::KeyCode::page_down
        || key_code == Widgets::KeyCode::escape
        || key_code == Widgets::KeyCode::tab
        || key_code == Widgets::KeyCode::enter
        || key_code == Widgets::KeyCode::backspace
        || key_code == Widgets::KeyCode::delete_forward
        || key_code == Widgets::KeyCode::f1
        || key_code == Widgets::KeyCode::f2
        || key_code == Widgets::KeyCode::f3
        || key_code == Widgets::KeyCode::f4
        || key_code == Widgets::KeyCode::f5
        || key_code == Widgets::KeyCode::f6
        || key_code == Widgets::KeyCode::f7
        || key_code == Widgets::KeyCode::f8
        || key_code == Widgets::KeyCode::f9
        || key_code == Widgets::KeyCode::f10
        || key_code == Widgets::KeyCode::f11
        || key_code == Widgets::KeyCode::f12;
}

// ---------------------------------------------------------------------------
// Process-wide X state
// ---------------------------------------------------------------------------

namespace
{
    Display*                     g_display = nullptr;
    float                        g_scale   = 1.0f;
    Widgets::IbusIme*            g_ime     = nullptr;
    Atom                         g_wm_delete = 0;
    std::vector<Widgets::x11::X11Window*> g_x11_windows;

    // Cursor shapes, created once per connection.
    struct Cursors
    {
        Cursor arrow = 0, pointer = 0, text = 0, forbidden = 0, resize_ns = 0, resize_ew = 0;
    } g_cursors;

    // X11 button numbering: Button1 = left (primary), Button2 = middle
    // (tertiary), Button3 = right (secondary). Button4/5 are the scroll
    // wheel, reported as synthetic buttons (no separate scroll handling yet):
    // map them to `unknown` rather than to a real button.
    Widgets::PointerButton buttonOf(unsigned int button)
    {
        switch (button)
        {
            case Button1: return Widgets::PointerButton::primary;
            case Button2: return Widgets::PointerButton::tertiary;
            case Button3: return Widgets::PointerButton::secondary;
            default:      return Widgets::PointerButton::unknown;
        }
    }

    // First UTF-8 code point of `buf` (XLookupString output), or 0.
    uint32_t decodeFirstCodePoint(const char* buf, int len)
    {
        if (len <= 0) return 0;
        const auto b = [buf](int i) { return static_cast<unsigned char>(buf[i]); };
        const unsigned char c = b(0);
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0 && len >= 2)
            return ((c & 0x1F) << 6) | (b(1) & 0x3F);
        if ((c & 0xF0) == 0xE0 && len >= 3)
            return ((c & 0x0F) << 12) | ((b(1) & 0x3F) << 6) | (b(2) & 0x3F);
        if ((c & 0xF8) == 0xF0 && len >= 4)
            return ((c & 0x07) << 18) | ((b(1) & 0x3F) << 12) | ((b(2) & 0x3F) << 6) | (b(3) & 0x3F);
        return 0;
    }
}

namespace systems::leal::campello_widgets::x11
{
    void init(Display* display, float display_scale)
    {
        g_display   = display;
        g_scale     = display_scale > 0.0f ? display_scale : 1.0f;
        g_wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
        g_cursors.arrow     = XCreateFontCursor(display, XC_left_ptr);
        g_cursors.pointer   = XCreateFontCursor(display, XC_hand2);
        g_cursors.text      = XCreateFontCursor(display, XC_xterm);
        g_cursors.forbidden = XCreateFontCursor(display, XC_X_cursor);
        g_cursors.resize_ns = XCreateFontCursor(display, XC_sb_v_double_arrow);
        g_cursors.resize_ew = XCreateFontCursor(display, XC_sb_h_double_arrow);
    }

    void shutdown()
    {
        if (!g_display) return;
        for (Cursor c : {g_cursors.arrow, g_cursors.pointer, g_cursors.text,
                         g_cursors.forbidden, g_cursors.resize_ns, g_cursors.resize_ew})
            if (c) XFreeCursor(g_display, c);
        g_cursors = {};
        g_display = nullptr;
        g_ime     = nullptr;
    }

    Display* display()      { return g_display; }
    float    displayScale() { return g_scale; }
    void     setIme(IbusIme* ime) { g_ime = ime; }
    IbusIme* ime()          { return g_ime; }

    // ----------------------------------------------------------------------
    // X11Window
    // ----------------------------------------------------------------------

    X11Window::X11Window(View& view, const WindowConfig& config)
        : LinuxWindow(view)
    {
        // X11 doesn't scale a client's buffer for HiDPI: the window is sized
        // in physical pixels, and the scale reported through MediaQuery.
        width_  = static_cast<uint32_t>(std::max(1L, std::lround(config.size.width  * g_scale)));
        height_ = static_cast<uint32_t>(std::max(1L, std::lround(config.size.height * g_scale)));

        XSetWindowAttributes swa = {};
        swa.event_mask = ExposureMask | StructureNotifyMask |
                         KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask;
        window_ = XCreateWindow(
            g_display, DefaultRootWindow(g_display),
            0, 0, width_, height_, 0,
            CopyFromParent, InputOutput, CopyFromParent,
            CWEventMask, &swa);
        if (!window_) return;
        g_x11_windows.push_back(this);

        XStoreName(g_display, window_, config.title.c_str());

        // WM_CLASS: lets window managers and desktop shells match this window
        // to an installed .desktop file (taskbar / alt-tab icon and name).
        if (!config.app_id.empty())
        {
            XClassHint* class_hint = XAllocClassHint();
            class_hint->res_name  = const_cast<char*>(config.app_id.c_str());
            class_hint->res_class = const_cast<char*>(config.app_id.c_str());
            XSetClassHint(g_display, window_, class_hint);
            XFree(class_hint);
        }

        XSetWMProtocols(g_display, window_, &g_wm_delete, 1);

        GPU::LinuxSurfaceInfo info{};
        info.display = g_display;
        info.window  = reinterpret_cast<void*>(static_cast<uintptr_t>(window_));
        info.api     = GPU::LinuxWindowApi::x11;
        info.width   = width_;
        info.height  = height_;
        attachSurface(info);
    }

    X11Window::~X11Window()
    {
        g_x11_windows.erase(std::remove(g_x11_windows.begin(), g_x11_windows.end(), this),
                            g_x11_windows.end());
        // The renderer, raster thread and surface (in ~LinuxWindow) go after
        // the X window would: tear them down first, while the window that
        // the Vulkan swapchain presents to still exists.
        setRoot(nullptr);
        if (window_ && g_display)
        {
            XDestroyWindow(g_display, window_);
            XFlush(g_display);
        }
    }

    void X11Window::show()
    {
        if (!window_) return;
        XMapWindow(g_display, window_);
        XFlush(g_display);
    }

    X11Window* X11Window::forXid(::Window window)
    {
        for (X11Window* w : g_x11_windows)
            if (w->window_ == window) return w;
        return nullptr;
    }

    void X11Window::setCursor(SystemMouseCursor c)
    {
        if (!window_) return;
        Cursor cur = g_cursors.arrow;
        switch (c)
        {
            case SystemMouseCursor::pointer:   cur = g_cursors.pointer;   break;
            case SystemMouseCursor::text:      cur = g_cursors.text;      break;
            case SystemMouseCursor::forbidden: cur = g_cursors.forbidden; break;
            case SystemMouseCursor::resize_ns: cur = g_cursors.resize_ns; break;
            case SystemMouseCursor::resize_ew: cur = g_cursors.resize_ew; break;
            default: break;
        }
        XDefineCursor(g_display, window_, cur);
        XFlush(g_display);
    }

    void X11Window::updateImeCursorPosition()
    {
        if (!g_ime || !g_ime->isActive()) return;
        TextInputManager& tim = view().textInputManager();
        if (!tim.hasInputTarget() || !tim.activeController()) return;

        auto rect = tim.getCharacterRect(tim.activeController()->selectionEnd());
        if (rect[2] <= 0.0f || rect[3] <= 0.0f) return;

        // `rect` is in logical pixels; the X window is sized in physical
        // pixels: scale up, then translate to root (screen) coordinates.
        ::Window root;
        int x_root = 0, y_root = 0;
        XTranslateCoordinates(g_display, window_, DefaultRootWindow(g_display),
            static_cast<int>(rect[0] * g_scale),
            static_cast<int>((rect[1] + rect[3]) * g_scale),
            &x_root, &y_root, &root);
        g_ime->setCursorLocation(x_root, y_root,
            static_cast<int>(rect[2] * g_scale),
            static_cast<int>(rect[3] * g_scale));
    }

    void X11Window::handleEvent(const XEvent& ev)
    {
        View::Scope scope(&view());
        PointerDispatcher& dispatcher = view().pointerDispatcher();
        const float scale = g_scale;

        switch (ev.type)
        {
            case ConfigureNotify:
            {
                const auto w = static_cast<uint32_t>(std::max(0, ev.xconfigure.width));
                const auto h = static_cast<uint32_t>(std::max(0, ev.xconfigure.height));
                if (w == 0 || h == 0 || (w == width_ && h == height_)) break;
                width_  = w;
                height_ = h;
                resizeSurface(w, h);
                notifyMetricsChanged();  // may destroy `this`
                break;
            }

            case MapNotify:
                mapped_ = true;
                if (renderer()) renderer()->notePaintRequested();
                requestFrame();
                break;

            case UnmapNotify:
                mapped_ = false;
                break;

            case Expose:
                // Without a compositor (plain X, Xvfb) the uncovered area has
                // no content until we draw again: repaint the whole window
                // once the last Expose of the series arrives.
                if (ev.xexpose.count == 0)
                {
                    if (renderer()) renderer()->notePaintRequested();
                    requestFrame();
                }
                break;

            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == g_wm_delete)
                {
                    auto callback = on_close;  // may destroy `this`
                    if (callback) callback();
                }
                break;

            case ButtonPress:
            case ButtonRelease:
            {
                const bool down = ev.type == ButtonPress;
                PointerEvent e;
                e.kind       = down ? PointerEventKind::down : PointerEventKind::up;
                e.pointer_id = 0;
                e.position   = { static_cast<float>(ev.xbutton.x) / scale,
                                 static_cast<float>(ev.xbutton.y) / scale };
                e.pressure   = down ? 1.0f : 0.0f;
                e.button     = buttonOf(ev.xbutton.button);
                mouse_pressed_ = down;
                dispatcher.handlePointerEvent(e);
                if (down && isLive(this)) updateImeCursorPosition();
                break;
            }

            case MotionNotify:
            {
                PointerEvent e;
                e.kind       = PointerEventKind::move;
                e.pointer_id = 0;
                e.position   = { static_cast<float>(ev.xmotion.x) / scale,
                                 static_cast<float>(ev.xmotion.y) / scale };
                e.pressure   = mouse_pressed_ ? 1.0f : 0.0f;
                dispatcher.handlePointerEvent(e);
                break;
            }

            case KeyPress:
            case KeyRelease:
                handleKey(ev.xkey, ev.type == KeyPress);
                break;

            default:
                break;
        }
    }

    void X11Window::handleKey(const XKeyEvent& key, bool press)
    {
        FocusManager& focus = view().focusManager();
        KeySym keysym = XkbKeycodeToKeysym(g_display, key.keycode, 0, 0);
        const KeyCode key_code = x11KeysymToKeyCode(keysym);
        const uint32_t mods = x11StateToKeyModifiers(key.state);

        KeyEvent ke;
        ke.key_code  = key_code;
        ke.modifiers = mods;
        ke.character = 0;

        if (!press)
        {
            ke.kind = KeyEventKind::up;
            focus.handleKeyEvent(ke);
            return;
        }
        ke.kind = KeyEventKind::down;

        // Navigation / special keys bypass the IME.
        if (isNavigationOrSpecialKeyX11(key_code))
        {
            focus.handleKeyEvent(ke);
            return;
        }

        // IBus first while a text field has the input target. A Ctrl-held
        // keystroke is never composition input (it's an app or system command
        // by convention), so it still reaches FocusManager::handleKeyEvent()
        // even then; otherwise app-wide Ctrl+<key> shortcuts (see
        // FocusManager::setGlobalKeyHandler()) would stop firing whenever a
        // TextField has focus.
        TextInputManager& tim = view().textInputManager();
        if (g_ime && g_ime->isActive() && tim.hasInputTarget() &&
            !(mods & KeyModifiers::ctrl))
        {
            if (g_ime->processKeyEvent(static_cast<uint32_t>(keysym),
                                       static_cast<uint32_t>(key.keycode), key.state))
                return;  // consumed for composition
        }

        char buf[16] = {};
        const int len = XLookupString(const_cast<XKeyEvent*>(&key), buf, sizeof(buf), nullptr, nullptr);
        ke.character = decodeFirstCodePoint(buf, len);
        focus.handleKeyEvent(ke);
    }

} // namespace systems::leal::campello_widgets::x11
