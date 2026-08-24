#include <campello_widgets/linux/run_app.hpp>
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
#include <campello_widgets/ui/raster_thread.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/platform/linux_surface.hpp>

#include "ibus_ime.hpp"
#include "linux_display_backend.hpp"
#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <dbus/dbus.h>

// C interface installed by src/linux/platform_menu_delegate.cpp — must run
// before the widget tree mounts so PlatformMenuBar::build()'s setMenus()
// and PlatformMenuBarView's needsInWindowMenuBar() check reach the real
// LinuxPlatformMenuDelegate instead of the default no-op.
extern "C" void campello_widgets_initialize_linux_menu_delegate();

#include <cctype>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

// Namespace aliases
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Forward declaration for Wayland runner (defined in wayland_runner.cpp)
// ---------------------------------------------------------------------------
#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
namespace systems::leal::campello_widgets {
    int runAppWayland(const std::string& title, int width, int height,
                      WidgetRef root_widget, bool resizable,
                      const std::string& app_id);
}
#endif

// ---------------------------------------------------------------------------
// Derives a default app_id from the window title when the caller doesn't
// supply one explicitly — lowercased, non-alphanumerics collapsed to '-'.
// Better than leaving the compositor with nothing, but callers that ship a
// .desktop file should pass an app_id matching its basename explicitly.
// ---------------------------------------------------------------------------
static std::string defaultAppIdFromTitle(const std::string& title)
{
    std::string result;
    result.reserve(title.size());
    bool last_was_dash = false;
    for (unsigned char c : title) {
        if (std::isalnum(c)) {
            result += static_cast<char>(std::tolower(c));
            last_was_dash = false;
        } else if (!last_was_dash && !result.empty()) {
            result += '-';
            last_was_dash = true;
        }
    }
    while (!result.empty() && result.back() == '-') result.pop_back();
    return result.empty() ? "campello-widgets-app" : result;
}

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
    std::string        gTitle;
    std::string        gAppId;
    int                gWidth  = 800;
    int                gHeight = 600;
    bool               gResizable = true;
}

// ---------------------------------------------------------------------------
// Window state
// ---------------------------------------------------------------------------
struct WindowState
{
    Display*                                    display = nullptr;
    Window                                      window  = 0;
    int                                         screen  = 0;
    std::shared_ptr<GPU::Device>                device;
    std::shared_ptr<Widgets::Renderer>          renderer;
    std::shared_ptr<Widgets::Element>           root_element;
    std::shared_ptr<Widgets::PointerDispatcher> dispatcher;
    std::shared_ptr<Widgets::FocusManager>      focus_manager;
    std::unique_ptr<Widgets::TickerScheduler>   ticker_scheduler;
    std::unique_ptr<Widgets::TextInputManager>  text_input_manager;
    std::unique_ptr<Widgets::IbusIme>           ibus_ime;
    std::unique_ptr<Widgets::RasterThread>      raster_thread;

    bool running = true;
    bool needs_redraw = true;
    bool mouse_pressed = false;
    float display_scale = 1.0f;
    Widgets::MediaQueryData                     media_data;
    Widgets::WidgetRef                          user_root_widget;
};

static WindowState* gWindowState = nullptr;

// Forward declaration — defined after createSession / runApp helpers.
static void rebuildMediaQuery(WindowState* state);

// ---------------------------------------------------------------------------
// Dark-mode D-Bus monitor (xdg-desktop-portal)
// ---------------------------------------------------------------------------

static DBusConnection* gDarkModeConn  = nullptr;
static WindowState*    gDarkModeState = nullptr;

static DBusHandlerResult darkModeDBusFilter(DBusConnection* /*connection*/,
                                              DBusMessage* msg,
                                              void* user_data)
{
    if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Settings", "SettingChanged"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessageIter iter;
    if (!dbus_message_iter_init(msg, &iter))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* ns = nullptr;
    dbus_message_iter_get_basic(&iter, &ns);

    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* key = nullptr;
    dbus_message_iter_get_basic(&iter, &key);

    if (!ns || !key || std::strcmp(ns, "org.freedesktop.appearance") != 0 ||
        std::strcmp(key, "color-scheme") != 0)
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessageIter variant;
    dbus_message_iter_recurse(&iter, &variant);

    uint32_t value = 0;
    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT32)
        dbus_message_iter_get_basic(&variant, &value);

    auto* state = static_cast<WindowState*>(user_data);
    Widgets::Brightness newBrightness = (value == 1)
        ? Widgets::Brightness::dark : Widgets::Brightness::light;
    if (state->media_data.platform_brightness != newBrightness)
    {
        state->media_data.platform_brightness = newBrightness;
        std::cerr << "[Linux] platform brightness changed to "
                  << (newBrightness == Widgets::Brightness::dark ? "dark" : "light") << "\n";
        rebuildMediaQuery(state);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static bool initializeDarkModeMonitor(WindowState* state)
{
    DBusError err;
    dbus_error_init(&err);

    gDarkModeConn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!gDarkModeConn || dbus_error_is_set(&err))
    {
        if (dbus_error_is_set(&err)) dbus_error_free(&err);
        return false;
    }

    dbus_connection_ref(gDarkModeConn);

    dbus_bus_add_match(gDarkModeConn,
        "type='signal',interface='org.freedesktop.portal.Settings',member='SettingChanged'",
        &err);
    if (dbus_error_is_set(&err))
    {
        std::cerr << "[Linux] Failed to add dark-mode signal match: " << err.message << "\n";
        dbus_error_free(&err);
        dbus_connection_unref(gDarkModeConn);
        gDarkModeConn = nullptr;
        return false;
    }

    gDarkModeState = state;
    dbus_connection_add_filter(gDarkModeConn, darkModeDBusFilter, state, nullptr);
    return true;
}

static void shutdownDarkModeMonitor()
{
    if (!gDarkModeConn) return;
    dbus_connection_remove_filter(gDarkModeConn, darkModeDBusFilter, gDarkModeState);
    gDarkModeState = nullptr;
    dbus_connection_unref(gDarkModeConn);
    gDarkModeConn = nullptr;
}

static void pumpDarkModeEvents()
{
    if (!gDarkModeConn) return;
    dbus_connection_read_write(gDarkModeConn, 0);
    while (dbus_connection_get_dispatch_status(gDarkModeConn) == DBUS_DISPATCH_DATA_REMAINS)
    {
        dbus_connection_dispatch(gDarkModeConn);
    }
}

static Widgets::Brightness getSystemBrightness()
{
    // Query the xdg-desktop-portal Settings interface for color-scheme.
    // Value: 0 = no preference, 1 = dark, 2 = light
    DBusError err;
    dbus_error_init(&err);

    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn || dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return Widgets::Brightness::light;
    }

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "Read");
    if (!msg) return Widgets::Brightness::light;

    const char* ns  = "org.freedesktop.appearance";
    const char* key = "color-scheme";
    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &ns,
        DBUS_TYPE_STRING, &key,
        DBUS_TYPE_INVALID);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, 500, &err);
    dbus_message_unref(msg);

    if (!reply || dbus_error_is_set(&err)) {
        if (reply) dbus_message_unref(reply);
        dbus_error_free(&err);
        return Widgets::Brightness::light;
    }

    DBusMessageIter iter;
    if (!dbus_message_iter_init(reply, &iter)) {
        dbus_message_unref(reply);
        return Widgets::Brightness::light;
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
        dbus_message_unref(reply);
        return Widgets::Brightness::light;
    }

    DBusMessageIter variant;
    dbus_message_iter_recurse(&iter, &variant);

    uint32_t value = 0;
    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&variant, &value);
    }

    dbus_message_unref(reply);
    // dbus_bus_get returns a shared connection — do not unref

    return (value == 1) ? Widgets::Brightness::dark : Widgets::Brightness::light;
}

static void rebuildMediaQuery(WindowState* state)
{
    if (!state || !state->root_element) return;
    auto newMediaQuery = std::make_shared<Widgets::MediaQuery>(
        state->media_data, state->user_root_widget);
    state->root_element->update(newMediaQuery);
    Widgets::FrameScheduler::scheduleFrame();
}

// ---------------------------------------------------------------------------
// HiDPI scale detection
// ---------------------------------------------------------------------------
// X11 has no automatic window-content scaling: unlike macOS/Windows, the
// server never stretches a client's buffer to match the display's true
// pixel density. A client that wants to look correct on a HiDPI screen must
// size its own window in real (physical) pixels and report the physical/
// logical ratio itself. The desktop-standard signal for that ratio is the
// `Xft.dpi` X resource — every major desktop environment's settings daemon
// publishes it (including for XWayland clients), so it's checked first.
// RandR/core-protocol physical screen size (mm) is a last-resort fallback
// for setups that never populate it.
static float getX11DisplayScale(Display* display, int screen)
{
    bool  found = false;
    float scale = 1.0f;

    char* resource_string = XResourceManagerString(display);
    if (resource_string) {
        XrmDatabase db = XrmGetStringDatabase(resource_string);
        if (db) {
            char*    type = nullptr;
            XrmValue value;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
                float dpi = static_cast<float>(std::atof(value.addr));
                if (dpi > 0.0f) {
                    scale = dpi / 96.0f;
                    found = true;
                }
            }
            XrmDestroyDatabase(db);
        }
    }

    if (!found) {
        int width_px = DisplayWidth(display, screen);
        int width_mm = DisplayWidthMM(display, screen);
        if (width_px > 0 && width_mm > 0) {
            float dpi = static_cast<float>(width_px) * 25.4f / static_cast<float>(width_mm);
            // Snap to the nearest quarter step — matches desktop-environment
            // scale presets (1.0, 1.25, 1.5, ...) and smooths out imprecise
            // EDID physical-size reporting that would otherwise produce
            // odd-looking float scales from this fallback path.
            scale = std::round((dpi / 96.0f) * 4.0f) / 4.0f;
        }
    }

    return scale < 1.0f ? 1.0f : scale;
}

// ---------------------------------------------------------------------------
// X11 keycode translation
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
// IME cursor position helper
// ---------------------------------------------------------------------------

static void updateImeCursorPosition(WindowState* state)
{
    if (!state || !state->ibus_ime || !state->ibus_ime->isActive()) return;
    if (!state->text_input_manager || !state->text_input_manager->hasInputTarget()) return;

    auto rect = state->text_input_manager->getCharacterRect(
        state->text_input_manager->activeController()->selectionEnd());

    if (rect[2] <= 0.0f || rect[3] <= 0.0f) return;

    // Convert from client coordinates to screen coordinates. `rect` is in
    // logical pixels (RenderBox tree space); the X11 window is sized in
    // physical pixels, so scale up before translating.
    Window root;
    int x_root, y_root;
    XTranslateCoordinates(state->display, state->window,
        DefaultRootWindow(state->display),
        static_cast<int>(rect[0] * state->display_scale),
        static_cast<int>((rect[1] + rect[3]) * state->display_scale),
        &x_root, &y_root, &root);

    state->ibus_ime->setCursorLocation(
        x_root, y_root,
        static_cast<int>(rect[2] * state->display_scale),
        static_cast<int>(rect[3] * state->display_scale));
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------

static void handleX11Event(WindowState* state, const XEvent& ev)
{
    switch (ev.type) {
        case ConfigureNotify: {
            if (ev.xconfigure.width > 0 && ev.xconfigure.height > 0) {
                gWidth  = ev.xconfigure.width;
                gHeight = ev.xconfigure.height;
                Widgets::MediaQueryData newData = state->media_data;
                newData.logical_size = Widgets::Size{
                    static_cast<float>(ev.xconfigure.width)  / state->display_scale,
                    static_cast<float>(ev.xconfigure.height) / state->display_scale };
                if (newData != state->media_data) {
                    state->media_data = newData;
                    rebuildMediaQuery(state);
                }
                state->needs_redraw = true;
            }
            break;
        }

        case Expose: {
            state->needs_redraw = true;
            break;
        }

        case ClientMessage: {
            Atom wm_delete = XInternAtom(state->display, "WM_DELETE_WINDOW", False);
            if (static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete) {
                state->running = false;
            }
            break;
        }

        case ButtonPress: {
            if (!state->dispatcher) break;
            int x = ev.xbutton.x;
            int y = ev.xbutton.y;

            Widgets::PointerEvent e;
            e.kind = Widgets::PointerEventKind::down;
            e.pointer_id = 0;
            e.position = { static_cast<float>(x) / state->display_scale,
                           static_cast<float>(y) / state->display_scale };
            e.pressure = 1.0f;
            state->dispatcher->handlePointerEvent(e);
            state->mouse_pressed = true;

            // Update IME cursor position on click
            updateImeCursorPosition(state);
            break;
        }

        case ButtonRelease: {
            if (!state->dispatcher) break;
            int x = ev.xbutton.x;
            int y = ev.xbutton.y;

            Widgets::PointerEvent e;
            e.kind = Widgets::PointerEventKind::up;
            e.pointer_id = 0;
            e.position = { static_cast<float>(x) / state->display_scale,
                           static_cast<float>(y) / state->display_scale };
            e.pressure = 0.0f;
            state->dispatcher->handlePointerEvent(e);
            state->mouse_pressed = false;
            break;
        }

        case MotionNotify: {
            if (!state->dispatcher) break;
            int x = ev.xmotion.x;
            int y = ev.xmotion.y;

            Widgets::PointerEvent e;
            e.kind = Widgets::PointerEventKind::move;
            e.pointer_id = 0;
            e.position = { static_cast<float>(x) / state->display_scale,
                           static_cast<float>(y) / state->display_scale };
            e.pressure = state->mouse_pressed ? 1.0f : 0.0f;
            state->dispatcher->handlePointerEvent(e);
            break;
        }

        case KeyPress: {
            if (!state->focus_manager) break;

            KeySym keysym = XkbKeycodeToKeysym(state->display,
                ev.xkey.keycode, 0, 0);
            Widgets::KeyCode key_code = x11KeysymToKeyCode(keysym);
            uint32_t mods = x11StateToKeyModifiers(ev.xkey.state);

            // Navigation / special keys bypass IME
            if (isNavigationOrSpecialKeyX11(key_code))
            {
                Widgets::KeyEvent ke;
                ke.kind      = Widgets::KeyEventKind::down;
                ke.key_code  = key_code;
                ke.modifiers = mods;
                ke.character = 0;
                state->focus_manager->handleKeyEvent(ke);
                break;
            }

            // Try IBus first if we have an active text input target. A
            // Ctrl-held keystroke is never text composition input — it's
            // always an app/system command by convention — so it must
            // still reach FocusManager::handleKeyEvent() below even while
            // a text field has IBus's input target, or app-wide
            // Ctrl+<key> shortcuts (see FocusManager::setGlobalKeyHandler())
            // would silently stop firing the moment any TextField gains
            // focus.
            bool consumed_by_ime = false;
            if (state->ibus_ime && state->ibus_ime->isActive() &&
                state->text_input_manager && state->text_input_manager->hasInputTarget() &&
                !(mods & Widgets::KeyModifiers::ctrl))
            {
                consumed_by_ime = state->ibus_ime->processKeyEvent(
                    static_cast<uint32_t>(keysym),
                    static_cast<uint32_t>(ev.xkey.keycode),
                    ev.xkey.state);
            }

            if (!consumed_by_ime)
            {
                // Get Unicode character
                char buf[16] = {};
                int len = XLookupString(const_cast<XKeyEvent*>(&ev.xkey),
                                        buf, sizeof(buf), nullptr, nullptr);
                uint32_t character = 0;
                if (len > 0) {
                    // Decode first UTF-8 codepoint
                    unsigned char c = static_cast<unsigned char>(buf[0]);
                    if (c < 0x80) {
                        character = c;
                    } else if ((c & 0xE0) == 0xC0 && len >= 2) {
                        character = ((c & 0x1F) << 6)
                                  | (static_cast<unsigned char>(buf[1]) & 0x3F);
                    } else if ((c & 0xF0) == 0xE0 && len >= 3) {
                        character = ((c & 0x0F) << 12)
                                  | ((static_cast<unsigned char>(buf[1]) & 0x3F) << 6)
                                  | (static_cast<unsigned char>(buf[2]) & 0x3F);
                    } else if ((c & 0xF8) == 0xF0 && len >= 4) {
                        character = ((c & 0x07) << 18)
                                  | ((static_cast<unsigned char>(buf[1]) & 0x3F) << 12)
                                  | ((static_cast<unsigned char>(buf[2]) & 0x3F) << 6)
                                  | (static_cast<unsigned char>(buf[3]) & 0x3F);
                    }
                }

                Widgets::KeyEvent ke;
                ke.kind      = Widgets::KeyEventKind::down;
                ke.key_code  = key_code;
                ke.modifiers = mods;
                ke.character = character;
                state->focus_manager->handleKeyEvent(ke);
            }
            break;
        }

        case KeyRelease: {
            if (!state->focus_manager) break;

            KeySym keysym = XkbKeycodeToKeysym(state->display,
                ev.xkey.keycode, 0, 0);
            Widgets::KeyCode key_code = x11KeysymToKeyCode(keysym);
            uint32_t mods = x11StateToKeyModifiers(ev.xkey.state);

            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::up;
            ke.key_code  = key_code;
            ke.modifiers = mods;
            ke.character = 0;
            state->focus_manager->handleKeyEvent(ke);
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Render frame
// ---------------------------------------------------------------------------

static void renderFrame(WindowState* state)
{
    if (!state || !state->renderer || !state->device) return;

    auto package = state->renderer->buildFrame(
        static_cast<float>(gWidth),
        static_cast<float>(gHeight));
    if (!package) return;

    // On Vulkan, getSwapchainTextureView() returns nullptr — campello_gpu's
    // beginRenderPass acquires the swapchain image automatically when target is null.
    package->target = state->device->getSwapchainTextureView();

    state->raster_thread->submit(std::move(*package));
}

// ---------------------------------------------------------------------------
// Main runApp implementation
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{
    namespace GPU     = ::systems::leal::campello_gpu;
    namespace Widgets = ::systems::leal::campello_widgets;

int runApp(const std::string& title, int width, int height, WidgetRef root_widget)
{
    return runApp(title, width, height, std::move(root_widget), true, defaultAppIdFromTitle(title));
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable)
{
    return runApp(title, width, height, std::move(root_widget), resizable, defaultAppIdFromTitle(title));
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable,
           const std::string& app_id)
{
    gRootWidget = std::move(root_widget);
    gTitle      = title;
    gAppId      = app_id;
    gWidth      = width;
    gHeight     = height;
    gResizable  = resizable;

    // -----------------------------------------------------------------------
    // Runtime backend selection: Wayland if WAYLAND_DISPLAY is set
    // -----------------------------------------------------------------------
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (wayland_display && wayland_display[0] != '\0') {
#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
        std::cerr << "[Linux] Wayland detected (" << wayland_display << "), trying Wayland backend.\n";
        setRunningUnderWayland(true);
        int result = runAppWayland(title, width, height, gRootWidget, resizable, gAppId);
        if (result != 2) return result; // 2 = GPU init failed, fall back to X11
        setRunningUnderWayland(false);
        std::cerr << "[Linux] Wayland GPU init failed; falling back to X11 (XWayland).\n";
#else
        std::cerr << "[Linux] Wayland display detected but Wayland support not compiled in.\n";
        std::cerr << "[Linux] Falling back to X11 (may fail under pure Wayland compositors).\n";
#endif
    }

    // -----------------------------------------------------------------------
    // Open X11 display
    // -----------------------------------------------------------------------
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "[Linux] Failed to open X display\n";
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    // -----------------------------------------------------------------------
    // Detect HiDPI scale and size the window in physical pixels
    // -----------------------------------------------------------------------
    const float x11_scale = getX11DisplayScale(display, screen);
    if (x11_scale != 1.0f) {
        std::cerr << "[Linux] Display scale: " << x11_scale << "x\n";
    }
    const int phys_width  = static_cast<int>(std::lround(width  * x11_scale));
    const int phys_height = static_cast<int>(std::lround(height * x11_scale));

    // -----------------------------------------------------------------------
    // Create X11 window
    // -----------------------------------------------------------------------
    XSetWindowAttributes swa = {};
    swa.event_mask = ExposureMask | StructureNotifyMask |
                     KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask;

    Window window = XCreateWindow(
        display, root,
        0, 0, static_cast<unsigned int>(phys_width), static_cast<unsigned int>(phys_height), 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWEventMask, &swa);

    XStoreName(display, window, gTitle.c_str());

    // WM_CLASS — lets window managers/desktop shells match this window to an
    // installed .desktop file for the taskbar/alt-tab icon and name.
    {
        XClassHint* class_hint = XAllocClassHint();
        class_hint->res_name  = const_cast<char*>(gAppId.c_str());
        class_hint->res_class = const_cast<char*>(gAppId.c_str());
        XSetClassHint(display, window, class_hint);
        XFree(class_hint);
    }

    // Register WM_DELETE_WINDOW protocol
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);

    XMapWindow(display, window);
    XFlush(display);

    // -----------------------------------------------------------------------
    // Cursor shapes + register the cursor-change handler
    // -----------------------------------------------------------------------
    // Without this, MouseRegion/TextField/etc. cursor changes are silent
    // no-ops on X11 (setSystemCursor() just calls into a null handler — see
    // system_mouse_cursor.cpp) and every widget shows the window's default
    // inherited cursor regardless of what's actually under the pointer.
    struct X11Cursors {
        Cursor arrow      = 0;
        Cursor pointer    = 0;
        Cursor text       = 0;
        Cursor forbidden  = 0;
        Cursor resize_ns  = 0;
        Cursor resize_ew  = 0;
    } x11_cursors;
    x11_cursors.arrow     = XCreateFontCursor(display, XC_left_ptr);
    x11_cursors.pointer   = XCreateFontCursor(display, XC_hand2);
    x11_cursors.text      = XCreateFontCursor(display, XC_xterm);
    x11_cursors.forbidden = XCreateFontCursor(display, XC_X_cursor);
    x11_cursors.resize_ns = XCreateFontCursor(display, XC_sb_v_double_arrow);
    x11_cursors.resize_ew = XCreateFontCursor(display, XC_sb_h_double_arrow);

    // Captured by value: these are X server resource IDs (opaque XIDs), not
    // pointers into this stack frame, so they stay valid for the lifetime
    // of `display` regardless of x11_cursors's own scope.
    Widgets::registerCursorHandler([display, window, x11_cursors](Widgets::SystemMouseCursor c) {
        Cursor cur = x11_cursors.arrow;
        switch (c) {
            case Widgets::SystemMouseCursor::pointer:   cur = x11_cursors.pointer;   break;
            case Widgets::SystemMouseCursor::text:      cur = x11_cursors.text;      break;
            case Widgets::SystemMouseCursor::forbidden: cur = x11_cursors.forbidden; break;
            case Widgets::SystemMouseCursor::resize_ns: cur = x11_cursors.resize_ns; break;
            case Widgets::SystemMouseCursor::resize_ew: cur = x11_cursors.resize_ew; break;
            default: break;
        }
        XDefineCursor(display, window, cur);
        XFlush(display);
    });

    // -----------------------------------------------------------------------
    // Create GPU device
    // -----------------------------------------------------------------------
    GPU::LinuxSurfaceInfo surfaceInfo{};
    surfaceInfo.display = display;
    surfaceInfo.window  = reinterpret_cast<void*>(window);
    surfaceInfo.api     = GPU::LinuxWindowApi::x11;

    auto device = GPU::Device::createDefaultDevice(&surfaceInfo);
    if (!device) {
        std::cerr << "[Linux] Failed to create campello_gpu device\n";
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }

    std::cerr << "[Linux] Device: " << device->getName()
              << "  Engine: " << GPU::Device::getEngineVersion() << "\n";

    // -----------------------------------------------------------------------
    // Create window state
    // -----------------------------------------------------------------------
    WindowState state;
    gWindowState = &state;
    state.display = display;
    state.window  = window;
    state.screen  = screen;
    state.device  = device;
    state.display_scale = x11_scale;

    // Window was created at physical-pixel size; keep gWidth/gHeight (which
    // feed buildFrame() and the swapchain query) in sync with that, not the
    // caller's originally-requested logical size.
    gWidth  = phys_width;
    gHeight = phys_height;

    // -----------------------------------------------------------------------
    // Create dispatcher and focus manager before mounting
    // -----------------------------------------------------------------------
    state.dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(state.dispatcher.get());

    state.focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(state.focus_manager.get());

    state.text_input_manager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(state.text_input_manager.get());

    state.ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(state.ticker_scheduler.get());

    // -----------------------------------------------------------------------
    // Set up dark-mode D-Bus monitor
    // -----------------------------------------------------------------------
    if (!initializeDarkModeMonitor(&state)) {
        std::cerr << "[Linux] xdg-desktop-portal not available; dark-mode live updates disabled.\n";
        // Non-fatal — startup detection still works
    }

    // -----------------------------------------------------------------------
    // Set up IBus IME
    // -----------------------------------------------------------------------
    state.ibus_ime = std::make_unique<Widgets::IbusIme>();
    if (!state.ibus_ime->create()) {
        std::cerr << "[Linux] IBus not available; IME composition disabled.\n";
        // Non-fatal — app works without IME
    }

    // Wire TextInputManager focus changes to IBus
    state.text_input_manager->setOnInputTargetChanged(
        [&state](bool has_target) {
            if (!state.ibus_ime || !state.ibus_ime->isActive()) return;
            if (has_target) {
                state.ibus_ime->focusIn();
                updateImeCursorPosition(&state);
            } else {
                state.ibus_ime->focusOut();
            }
        });

    // -----------------------------------------------------------------------
    // FrameScheduler callback
    // -----------------------------------------------------------------------
    Widgets::FrameScheduler::setCallback([&state]() {
        state.needs_redraw = true;
    });

    // -----------------------------------------------------------------------
    // Wrap root widget with MediaQuery and mount
    // -----------------------------------------------------------------------
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = x11_scale;
    mediaData.platform_brightness = getSystemBrightness();
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(width),
        static_cast<float>(height) };
    state.media_data = mediaData;
    state.user_root_widget = gRootWidget;

    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(mediaData, gRootWidget);

    // Bind the UI thread before any widget tree mutation.
    Widgets::ThreadChecker::instance().bindToCurrentThread();

    campello_widgets_initialize_linux_menu_delegate();

    state.root_element = wrappedRoot->createElement();
    state.root_element->mount(nullptr);

    auto* roe = state.root_element->findDescendantRenderObjectElement();
    if (!roe) {
        std::cerr << "[Linux] Widget tree produced no RenderObjectElement\n";
        return 1;
    }

    auto render_box = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!render_box) {
        std::cerr << "[Linux] Root render object is not a RenderBox\n";
        return 1;
    }

    state.dispatcher->setRoot(render_box);

    // -----------------------------------------------------------------------
    // Create Vulkan draw backend and renderer
    // -----------------------------------------------------------------------
    const Widgets::Color bgColor = Widgets::Color::white();
    const GPU::PixelFormat pixelFmt = GPU::PixelFormat::bgra8unorm;

    auto backendOwned = std::make_unique<Widgets::VulkanDrawBackend>(
        device, bgColor, pixelFmt);

    state.renderer = std::make_shared<Widgets::Renderer>(
        device, render_box, bgColor);
    state.renderer->setDrawBackend(std::move(backendOwned));
    // Renderer::device_pixel_ratio_ is distinct from MediaQueryData's copy —
    // it's what buildFrame() actually divides the physical viewport by to
    // get logical layout constraints. Without this, layout sizes everything
    // against the full physical viewport as if DPR were 1, shrinking every
    // fixed-size widget to roughly 1/scale of its intended on-screen size.
    state.renderer->setDevicePixelRatio(x11_scale);

    state.raster_thread = std::make_unique<Widgets::RasterThread>(
        [renderer = state.renderer](const Widgets::FramePackage& pkg) {
            renderer->rasterFrame(pkg);
        });
    state.raster_thread->start();

    // Initial draw
    state.needs_redraw = true;

    // -----------------------------------------------------------------------
    // Main event loop
    // -----------------------------------------------------------------------
    while (state.running) {
        // Pump X11 events
        while (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);
            handleX11Event(&state, ev);
        }

        // Pump dark-mode D-Bus signals
        pumpDarkModeEvents();

        // Pump IBus D-Bus signals
        if (state.ibus_ime && state.ibus_ime->isActive()) {
            state.ibus_ime->dispatchEvents();
        }

        // Render if needed
        if (state.needs_redraw) {
            state.needs_redraw = false;
            renderFrame(&state);
        }

        // Small sleep to avoid busy-waiting when idle
        if (!state.needs_redraw && XPending(display) == 0) {
            usleep(1000); // 1 ms
        }
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    shutdownDarkModeMonitor();
    state.ibus_ime.reset();

    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TextInputManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    // Drop the handler before freeing the cursors/closing the display it
    // captured — nothing should call setSystemCursor() after this point,
    // but leaving it registered would dangle otherwise.
    Widgets::registerCursorHandler(nullptr);
    XFreeCursor(display, x11_cursors.arrow);
    XFreeCursor(display, x11_cursors.pointer);
    XFreeCursor(display, x11_cursors.text);
    XFreeCursor(display, x11_cursors.forbidden);
    XFreeCursor(display, x11_cursors.resize_ns);
    XFreeCursor(display, x11_cursors.resize_ew);

    // Unmount element / render-object tree before tearing down GPU resources.
    state.root_element.reset();

    // Stop the raster thread before releasing the renderer — stop() blocks
    // until any in-flight rasterFrame() call completes and joins the thread,
    // so no raster work can ever observe a half-destroyed Renderer/Device.
    state.raster_thread.reset();

    state.renderer.reset();

    // Release everything else that can hold GPU-backed textures before the
    // device is destroyed. render_box is a local variable that would otherwise
    // outlive state.device; state.dispatcher holds a ref to it via setRoot().
    // OffsetLayer caches DrawLists that contain DrawImageCmd with
    // shared_ptr<Texture> — those textures call vkFreeMemory in their dtor.
    render_box.reset();
    state.dispatcher.reset();
    state.focus_manager.reset();
    Widgets::ImageCache::instance().clear();

    state.device.reset();

    XDestroyWindow(display, window);
    XCloseDisplay(display);

    gWindowState = nullptr;
    return 0;
}

} // namespace systems::leal::campello_widgets
