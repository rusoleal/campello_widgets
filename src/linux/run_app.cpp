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

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/platform/linux_surface.hpp>

#include "ibus_ime.hpp"
#include "vulkan_draw_backend.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <dbus/dbus.h>

#include <chrono>
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
#ifdef CAMPHELLO_WIDGETS_HAS_WAYLAND
namespace systems::leal::campello_widgets {
    int runAppWayland(const std::string& title, int width, int height,
                      WidgetRef root_widget, bool resizable);
}
#endif

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
    std::string        gTitle;
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

    bool running = true;
    bool needs_redraw = true;
    bool mouse_pressed = false;
    Widgets::MediaQueryData                     media_data;
    Widgets::WidgetRef                          user_root_widget;
};

static WindowState* gWindowState = nullptr;

// Forward declaration — defined after createSession / runApp helpers.
static void rebuildMediaQuery(WindowState* state);

// ---------------------------------------------------------------------------
// Dark-mode D-Bus monitor (xdg-desktop-portal)
// ---------------------------------------------------------------------------

static DBusConnection* gDarkModeConn = nullptr;

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

    dbus_connection_add_filter(gDarkModeConn, darkModeDBusFilter, state, nullptr);
    return true;
}

static void shutdownDarkModeMonitor()
{
    if (!gDarkModeConn) return;
    dbus_connection_remove_filter(gDarkModeConn, darkModeDBusFilter, nullptr);
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

    // Convert from client coordinates to screen coordinates
    Window root;
    int x_root, y_root;
    XTranslateCoordinates(state->display, state->window,
        DefaultRootWindow(state->display),
        static_cast<int>(rect[0]), static_cast<int>(rect[1] + rect[3]),
        &x_root, &y_root, &root);

    state->ibus_ime->setCursorLocation(
        x_root, y_root,
        static_cast<int>(rect[2]), static_cast<int>(rect[3]));
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
                    static_cast<float>(ev.xconfigure.width),
                    static_cast<float>(ev.xconfigure.height) };
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
            e.position = { static_cast<float>(x), static_cast<float>(y) };
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
            e.position = { static_cast<float>(x), static_cast<float>(y) };
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
            e.position = { static_cast<float>(x), static_cast<float>(y) };
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

            // Try IBus first if we have an active text input target
            bool consumed_by_ime = false;
            if (state->ibus_ime && state->ibus_ime->isActive() &&
                state->text_input_manager && state->text_input_manager->hasInputTarget())
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

    auto color_view = state->device->getSwapchainTextureView();
    if (!color_view) return;

    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
    if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);

    if (auto* backend = state->renderer->drawBackend()) {
        backend->setViewport(static_cast<float>(gWidth), static_cast<float>(gHeight));
    }

    state->renderer->renderFrame(
        color_view,
        static_cast<float>(gWidth),
        static_cast<float>(gHeight));
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
    return runApp(title, width, height, std::move(root_widget), true);
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable)
{
    gRootWidget = std::move(root_widget);
    gTitle      = title;
    gWidth      = width;
    gHeight     = height;
    gResizable  = resizable;

    // -----------------------------------------------------------------------
    // Runtime backend selection: Wayland if WAYLAND_DISPLAY is set
    // -----------------------------------------------------------------------
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (wayland_display && wayland_display[0] != '\0') {
#ifdef CAMPHELLO_WIDGETS_HAS_WAYLAND
        std::cerr << "[Linux] Wayland detected (" << wayland_display << "), using Wayland backend.\n";
        return runAppWayland(title, width, height, gRootWidget, resizable);
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
    // Create X11 window
    // -----------------------------------------------------------------------
    XSetWindowAttributes swa = {};
    swa.event_mask = ExposureMask | StructureNotifyMask |
                     KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask;

    Window window = XCreateWindow(
        display, root,
        0, 0, static_cast<unsigned int>(width), static_cast<unsigned int>(height), 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWEventMask, &swa);

    XStoreName(display, window, gTitle.c_str());

    // Register WM_DELETE_WINDOW protocol
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);

    XMapWindow(display, window);
    XFlush(display);

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
    // X11 doesn't have a built-in DPR concept; use 1.0 as default
    mediaData.device_pixel_ratio = 1.0f;
    mediaData.platform_brightness = getSystemBrightness();
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(width),
        static_cast<float>(height) };
    state.media_data = mediaData;
    state.user_root_widget = gRootWidget;

    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(mediaData, gRootWidget);

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

    state.renderer.reset();
    state.device.reset();

    XDestroyWindow(display, window);
    XCloseDisplay(display);

    gWindowState = nullptr;
    return 0;
}

} // namespace systems::leal::campello_widgets
