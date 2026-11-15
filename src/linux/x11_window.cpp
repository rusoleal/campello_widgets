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
#include <X11/Xatom.h>

#include <dlfcn.h>
#include <unistd.h>

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
    Display*                              g_display = nullptr;
    float                                 g_scale   = 1.0f;
    float                                 g_refresh = 60.0f;
    std::string                           g_app_id;
    Widgets::IbusIme*                     g_ime     = nullptr;
    std::vector<Widgets::x11::X11Window*> g_x11_windows;

    struct Atoms
    {
        Atom wm_protocols = 0, wm_delete = 0, net_wm_name = 0, utf8_string = 0, net_wm_pid = 0;
        Atom net_wm_window_type = 0, type_normal = 0, type_dialog = 0, type_popup_menu = 0;
        Atom net_wm_state = 0, state_modal = 0, state_skip_taskbar = 0, net_active_window = 0;
    } g_atoms;

    // Cursor shapes, created once per connection.
    struct Cursors
    {
        Cursor arrow = 0, pointer = 0, text = 0, forbidden = 0, resize_ns = 0, resize_ew = 0;
    } g_cursors;

    // The refresh rate through XRandR's RandR 1.1 calls, loaded at run time
    // so libXrandr's headers aren't a build dependency. The library stays
    // loaded for the process (see the end of the function).
    float queryRefreshRate(Display* display)
    {
        void* lib = dlopen("libXrandr.so.2", RTLD_LAZY | RTLD_LOCAL);
        if (!lib) return 60.0f;
        using GetInfo  = void* (*)(Display*, ::Window);
        using Rate     = short (*)(void*);
        using FreeInfo = void (*)(void*);
        auto get_info  = reinterpret_cast<GetInfo>(dlsym(lib, "XRRGetScreenInfo"));
        auto rate_of   = reinterpret_cast<Rate>(dlsym(lib, "XRRConfigCurrentRate"));
        auto free_info = reinterpret_cast<FreeInfo>(dlsym(lib, "XRRFreeScreenConfigInfo"));
        float hz = 60.0f;
        if (get_info && rate_of && free_info)
        {
            if (void* info = get_info(display, DefaultRootWindow(display)))
            {
                const short rate = rate_of(info);
                if (rate >= 20 && rate <= 500) hz = static_cast<float>(rate);
                free_info(info);
            }
        }
        // Never dlclose(): XRandR registers event hooks on the Display
        // (XESetWireToEvent), and Xlib would call into the unloaded library
        // on the next RandR event (a crash in _XEnq).
        return hz;
    }

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

    void setAtomProperty(::Window w, Atom property, const std::vector<Atom>& values)
    {
        XChangeProperty(g_display, w, property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(values.data()),
                        static_cast<int>(values.size()));
    }

    // Top-left of `w`'s content in root (screen) coordinates.
    bool rootOrigin(::Window w, int& x, int& y)
    {
        ::Window child;
        return XTranslateCoordinates(g_display, w, DefaultRootWindow(g_display), 0, 0, &x, &y, &child);
    }
}

namespace systems::leal::campello_widgets::x11
{
    void init(Display* display, float display_scale, std::string app_id)
    {
        g_display = display;
        g_scale   = display_scale > 0.0f ? display_scale : 1.0f;
        g_app_id  = std::move(app_id);
        g_refresh = queryRefreshRate(display);

        auto atom = [display](const char* name) { return XInternAtom(display, name, False); };
        g_atoms.wm_protocols       = atom("WM_PROTOCOLS");
        g_atoms.wm_delete          = atom("WM_DELETE_WINDOW");
        g_atoms.net_wm_name        = atom("_NET_WM_NAME");
        g_atoms.utf8_string        = atom("UTF8_STRING");
        g_atoms.net_wm_pid         = atom("_NET_WM_PID");
        g_atoms.net_wm_window_type = atom("_NET_WM_WINDOW_TYPE");
        g_atoms.type_normal        = atom("_NET_WM_WINDOW_TYPE_NORMAL");
        g_atoms.type_dialog        = atom("_NET_WM_WINDOW_TYPE_DIALOG");
        g_atoms.type_popup_menu    = atom("_NET_WM_WINDOW_TYPE_POPUP_MENU");
        g_atoms.net_wm_state       = atom("_NET_WM_STATE");
        g_atoms.state_modal        = atom("_NET_WM_STATE_MODAL");
        g_atoms.state_skip_taskbar = atom("_NET_WM_STATE_SKIP_TASKBAR");
        g_atoms.net_active_window  = atom("_NET_ACTIVE_WINDOW");

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
    float    refreshRate()  { return g_refresh; }
    void     setIme(IbusIme* ime) { g_ime = ime; }
    IbusIme* ime()          { return g_ime; }

    // ----------------------------------------------------------------------
    // X11Window
    // ----------------------------------------------------------------------

    X11Window::X11Window(View& view, const WindowConfig& config)
        : LinuxWindow(view)
        , kind_(config.kind)
        , owner_(config.owner)
        , resizable_(config.resizable && config.kind != WindowKind::popup)
        , position_(config.position)
    {
        // X11 doesn't scale a client's buffer for HiDPI: the window is sized
        // in physical pixels, and the scale reported through MediaQuery.
        width_  = static_cast<uint32_t>(std::max(1L, std::lround(config.size.width  * g_scale)));
        height_ = static_cast<uint32_t>(std::max(1L, std::lround(config.size.height * g_scale)));

        const bool popup = kind_ == WindowKind::popup;
        XSetWindowAttributes swa = {};
        swa.event_mask = ExposureMask | StructureNotifyMask | FocusChangeMask |
                         KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask | LeaveWindowMask;
        // A popup (menu) is placed by us and isn't managed or decorated by
        // the window manager.
        swa.override_redirect = popup ? True : False;
        window_ = XCreateWindow(
            g_display, DefaultRootWindow(g_display),
            0, 0, width_, height_, 0,
            CopyFromParent, InputOutput, CopyFromParent,
            CWEventMask | CWOverrideRedirect, &swa);
        if (!window_) return;
        g_x11_windows.push_back(this);

        setTitle(config.title);

        // WM_CLASS: lets window managers and desktop shells match this window
        // to an installed .desktop file (taskbar / alt-tab icon and name).
        if (!g_app_id.empty())
        {
            XClassHint* class_hint = XAllocClassHint();
            class_hint->res_name  = const_cast<char*>(g_app_id.c_str());
            class_hint->res_class = const_cast<char*>(g_app_id.c_str());
            XSetClassHint(g_display, window_, class_hint);
            XFree(class_hint);
        }
        const long pid = static_cast<long>(getpid());
        XChangeProperty(g_display, window_, g_atoms.net_wm_pid, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&pid), 1);

        XSetWMProtocols(g_display, window_, &g_atoms.wm_delete, 1);

        // Window type and state hints (EWMH), and the owner (ICCCM).
        if (popup)
        {
            setAtomProperty(window_, g_atoms.net_wm_window_type, {g_atoms.type_popup_menu});
        }
        else if (kind_ == WindowKind::dialog || config.modal)
        {
            setAtomProperty(window_, g_atoms.net_wm_window_type, {g_atoms.type_dialog});
            std::vector<Atom> state{g_atoms.state_skip_taskbar};
            if (config.modal) state.push_back(g_atoms.state_modal);
            setAtomProperty(window_, g_atoms.net_wm_state, state);
        }
        else
        {
            setAtomProperty(window_, g_atoms.net_wm_window_type, {g_atoms.type_normal});
        }
        if (owner_ && !popup) XSetTransientForHint(g_display, window_, owner_);
        applySizeHints();

        GPU::LinuxSurfaceInfo info{};
        info.display = g_display;
        info.window  = reinterpret_cast<void*>(static_cast<uintptr_t>(window_));
        info.api     = GPU::LinuxWindowApi::x11;
        info.width   = width_;
        info.height  = height_;
        attachSurface(info);

        // Paced to the display's refresh rate (X11 has no frame callbacks).
        setMinFrameInterval(std::chrono::nanoseconds(
            static_cast<long long>(1e9 / static_cast<double>(g_refresh))));

        // This window feeds the shared IBus context while one of its text
        // fields has the input target and the window has keyboard focus.
        view.textInputManager().setOnInputTargetChanged([this](bool has_target) {
            if (!g_ime || !g_ime->isActive() || !focused_) return;
            if (has_target) { g_ime->focusIn(); updateImeCursorPosition(); }
            else            g_ime->focusOut();
        });
    }

    X11Window::~X11Window()
    {
        g_x11_windows.erase(std::remove(g_x11_windows.begin(), g_x11_windows.end(), this),
                            g_x11_windows.end());
        if (View::isAlive(&view())) view().textInputManager().setOnInputTargetChanged(nullptr);
        if (focused_ && g_ime && g_ime->isActive()) g_ime->focusOut();
        // The renderer, raster thread and surface (in ~LinuxWindow) would go
        // after the X window: tear them down first, while the window the
        // Vulkan swapchain presents to still exists.
        setRoot(nullptr);
        if (window_ && g_display)
        {
            // A popup or dialog with focus: give it back to its owner, or X
            // reverts it to the root window (no window would get keys).
            if (focused_ && owner_ && forXid(owner_) && forXid(owner_)->isShown())
                XSetInputFocus(g_display, owner_, RevertToParent, CurrentTime);
            XDestroyWindow(g_display, window_);
            XFlush(g_display);
        }
    }

    void X11Window::setTitle(const std::string& title)
    {
        if (!window_) return;
        // _NET_WM_NAME is UTF-8 (EWMH); WM_NAME is Latin-1 for old window managers.
        XChangeProperty(g_display, window_, g_atoms.net_wm_name, g_atoms.utf8_string, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
        XStoreName(g_display, window_, title.c_str());
    }

    void X11Window::applySizeHints()
    {
        XSizeHints* hints = XAllocSizeHints();
        if (!resizable_)
        {
            hints->flags     |= PMinSize | PMaxSize;
            hints->min_width  = hints->max_width  = static_cast<int>(width_);
            hints->min_height = hints->max_height = static_cast<int>(height_);
        }
        if (placed_)
        {
            // Asked-for position: honoured instead of the window manager's placement.
            hints->flags |= USPosition | PPosition;
        }
        XSetWMNormalHints(g_display, window_, hints);
        XFree(hints);
    }

    void X11Window::setLogicalSize(Size size)
    {
        if (!window_) return;
        const auto w = static_cast<uint32_t>(std::max(1L, std::lround(size.width  * g_scale)));
        const auto h = static_cast<uint32_t>(std::max(1L, std::lround(size.height * g_scale)));
        if (!resizable_) { width_ = w; height_ = h; applySizeHints(); }
        XResizeWindow(g_display, window_, w, h);
        XFlush(g_display);
        // width_/height_ follow ConfigureNotify (the window manager may adjust).
    }

    void X11Window::place()
    {
        placed_ = true;
        const X11Window* owner = owner_ ? forXid(owner_) : nullptr;
        int ox = 0, oy = 0;
        const bool have_owner = owner && rootOrigin(owner_, ox, oy);

        int x = 0, y = 0;
        if (kind_ == WindowKind::popup && have_owner)
        {
            // `position` is relative to the owner's content.
            const Offset at = position_.value_or(Offset{0.0f, 0.0f});
            x = ox + static_cast<int>(std::lround(at.x * g_scale));
            y = oy + static_cast<int>(std::lround(at.y * g_scale));
        }
        else if (position_)
        {
            x = static_cast<int>(std::lround(position_->x * g_scale));
            y = static_cast<int>(std::lround(position_->y * g_scale));
        }
        else if (have_owner)
        {
            // Centred on the owner.
            x = ox + (static_cast<int>(owner->width_) - static_cast<int>(width_)) / 2;
            y = oy + (static_cast<int>(owner->height_) - static_cast<int>(height_)) / 2;
        }
        else
        {
            placed_ = false;  // the window manager decides
            return;
        }
        XMoveWindow(g_display, window_, x, y);
        applySizeHints();
    }

    void X11Window::show(bool activate_it)
    {
        if (!window_) return;
        // An owned window (dialog, popup) is placed relative to its owner:
        // while the owner isn't on screen yet (a window opened as the app
        // starts, before the window manager has placed the main window),
        // wait for the owner's MapNotify.
        if (X11Window* owner = owner_ ? forXid(owner_) : nullptr; owner && !owner->mapped_)
        {
            pending_show_     = true;
            pending_activate_ = activate_it;
            return;
        }
        pending_show_ = false;
        if (!placed_) place();
        XMapRaised(g_display, window_);
        show_called_ = true;
        raiseOwnedWindows();
        // Focus can only be set on a viewable window: after MapNotify.
        focus_on_map_ = activate_it;
        if (activate_it && mapped_) activate();
        XFlush(g_display);
    }

    void X11Window::hide()
    {
        if (!window_) return;
        XUnmapWindow(g_display, window_);
        XFlush(g_display);
    }

    void X11Window::raiseOwnedWindows()
    {
        // Keep the windows this one owns (dialogs, popups) above it, as a
        // window manager stacks transient windows; without one (Xvfb, bare
        // X) raising the owner would cover them.
        for (X11Window* w : g_x11_windows)
            if (w != this && w->owner_ == window_ && w->show_called_) XRaiseWindow(g_display, w->window_);
    }

    void X11Window::activate()
    {
        if (!window_) return;
        XRaiseWindow(g_display, window_);
        raiseOwnedWindows();
        if (kind_ != WindowKind::popup)
        {
            // Ask the window manager (EWMH), which owns focus for managed windows.
            XEvent ev = {};
            ev.xclient.type         = ClientMessage;
            ev.xclient.window       = window_;
            ev.xclient.message_type = g_atoms.net_active_window;
            ev.xclient.format       = 32;
            ev.xclient.data.l[0]    = 1;  // source: application
            ev.xclient.data.l[1]    = CurrentTime;
            XSendEvent(g_display, DefaultRootWindow(g_display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        }
        if (mapped_) XSetInputFocus(g_display, window_, RevertToParent, CurrentTime);
        XFlush(g_display);
    }

    X11Window* X11Window::forXid(::Window window)
    {
        for (X11Window* w : g_x11_windows)
            if (w->window_ == window) return w;
        return nullptr;
    }

    X11Window* X11Window::forView(const View* view)
    {
        for (X11Window* w : g_x11_windows)
            if (&w->view() == view) return w;
        return nullptr;
    }

    bool X11Window::anyOtherShown(const X11Window* except)
    {
        for (X11Window* w : g_x11_windows)
            if (w != except && w->kind_ != WindowKind::popup && w->mapped_) return true;
        return false;
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

    void X11Window::redirectToBlocker()
    {
        View* blocker = view().inputBlocker();
        if (X11Window* modal = blocker ? forView(blocker) : nullptr)
        {
            modal->activate();
            XBell(g_display, 0);
            XFlush(g_display);
        }
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
            {
                mapped_ = true;
                if (renderer()) renderer()->notePaintRequested();
                requestFrame();
                if (focus_on_map_)
                {
                    focus_on_map_ = false;
                    activate();
                }
                // Windows of ours that waited for this one (see show()).
                const std::vector<X11Window*> windows = g_x11_windows;
                for (X11Window* w : windows)
                    if (isLive(w) && w->owner_ == window_ && w->pending_show_) w->show(w->pending_activate_);
                break;
            }

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

            case FocusIn:
                // Focus moving between our own subwindows doesn't count.
                if (ev.xfocus.detail == NotifyPointer || ev.xfocus.detail == NotifyInferior) break;
                focused_   = true;
                had_focus_ = true;
                if (g_ime && g_ime->isActive() && view().textInputManager().hasInputTarget())
                {
                    g_ime->focusIn();
                    updateImeCursorPosition();
                }
                break;

            case FocusOut:
            {
                if (ev.xfocus.detail == NotifyPointer || ev.xfocus.detail == NotifyInferior) break;
                // Grabs (a window manager's move/resize, a menu) briefly take
                // focus and give it back: not a loss.
                if (ev.xfocus.mode == NotifyGrab || ev.xfocus.mode == NotifyUngrab) break;
                focused_ = false;
                if (g_ime && g_ime->isActive() && view().textInputManager().hasInputTarget())
                    g_ime->focusOut();
                if (had_focus_ && on_focus_lost)
                {
                    // Delivered on a later turn: the handler usually closes
                    // (destroys) this window.
                    X11Window* self = this;
                    linux_host::postTask([self] {
                        if (isLive(self) && !self->focused_ && self->on_focus_lost) self->on_focus_lost();
                    });
                }
                break;
            }

            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.message_type) == g_atoms.wm_protocols &&
                    static_cast<Atom>(ev.xclient.data.l[0]) == g_atoms.wm_delete)
                {
                    // Delivered on a later turn, like every platform's close
                    // request: the handler may destroy this window.
                    X11Window* self = this;
                    linux_host::postTask([self] {
                        if (!isLive(self)) return;
                        auto callback = self->on_close;
                        if (callback) callback();
                    });
                }
                break;

            case ButtonPress:
            case ButtonRelease:
            {
                const bool down = ev.type == ButtonPress;
                if (down && view().isInputBlocked()) redirectToBlocker();
                PointerEvent e;
                e.kind       = down ? PointerEventKind::down : PointerEventKind::up;
                e.pointer_id = 0;
                e.position   = { static_cast<float>(ev.xbutton.x) / scale,
                                 static_cast<float>(ev.xbutton.y) / scale };
                e.pressure   = down ? 1.0f : 0.0f;
                e.button     = buttonOf(ev.xbutton.button);
                e.device_kind = PointerDeviceKind::mouse;
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
                e.device_kind = PointerDeviceKind::mouse;
                dispatcher.handlePointerEvent(e);
                break;
            }

            case LeaveNotify:
            {
                // The pointer left this window (to another one, ours or not):
                // a move far outside ends its hovers. Not while a button is
                // held: X keeps delivering a drag to the window it began in.
                if (mouse_pressed_ || ev.xcrossing.mode != NotifyNormal) break;
                PointerEvent e;
                e.kind        = PointerEventKind::move;
                e.pointer_id  = 0;
                e.position    = { -100000.0f, -100000.0f };
                e.pressure    = 0.0f;
                e.device_kind = PointerDeviceKind::mouse;
                dispatcher.handlePointerEvent(e);
                break;
            }

            case KeyPress:
            case KeyRelease:
                if (ev.type == KeyPress && view().isInputBlocked()) redirectToBlocker();
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
