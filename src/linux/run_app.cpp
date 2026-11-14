#include <campello_widgets/linux/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

#include <campello_gpu/device.hpp>

#include "ibus_ime.hpp"
#include "linux_display_backend.hpp"
#include "linux_host.hpp"
#include "x11_window.hpp"

#include <X11/Xlib.h>
#include <X11/Xresource.h>

#include <dbus/dbus.h>

// C interface installed by src/linux/platform_menu_delegate.cpp — must run
// before the widget tree mounts so PlatformMenuBar::build()'s setMenus()
// and PlatformMenuBarView's needsInWindowMenuBar() check reach the real
// LinuxPlatformMenuDelegate instead of the default no-op.
extern "C" void campello_widgets_initialize_linux_menu_delegate();

#include <poll.h>
#include <signal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

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
// Dark-mode D-Bus monitor (xdg-desktop-portal)
// ---------------------------------------------------------------------------

static DBusConnection*                            gDarkModeConn = nullptr;
static std::function<void(Widgets::Brightness)>   gOnBrightnessChanged;

static DBusHandlerResult darkModeDBusFilter(DBusConnection* /*connection*/,
                                              DBusMessage* msg,
                                              void* /*user_data*/)
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

    const Widgets::Brightness brightness = (value == 1)
        ? Widgets::Brightness::dark : Widgets::Brightness::light;
    if (gOnBrightnessChanged) gOnBrightnessChanged(brightness);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static bool initializeDarkModeMonitor(std::function<void(Widgets::Brightness)> on_changed)
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

    gOnBrightnessChanged = std::move(on_changed);
    dbus_connection_add_filter(gDarkModeConn, darkModeDBusFilter, nullptr, nullptr);
    return true;
}

static void shutdownDarkModeMonitor()
{
    if (!gDarkModeConn) return;
    dbus_connection_remove_filter(gDarkModeConn, darkModeDBusFilter, nullptr);
    gOnBrightnessChanged = nullptr;
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
    // -----------------------------------------------------------------------
    // Runtime backend selection: Wayland if WAYLAND_DISPLAY is set
    // -----------------------------------------------------------------------
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (wayland_display && wayland_display[0] != '\0') {
#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
        std::cerr << "[Linux] Wayland detected (" << wayland_display << "), trying Wayland backend.\n";
        setRunningUnderWayland(true);
        int result = runAppWayland(title, width, height, root_widget, resizable, app_id);
        if (result != 2) return result; // 2 = GPU init failed, fall back to X11
        setRunningUnderWayland(false);
        std::cerr << "[Linux] Wayland GPU init failed; falling back to X11 (XWayland).\n";
#else
        std::cerr << "[Linux] Wayland display detected but Wayland support not compiled in.\n";
        std::cerr << "[Linux] Falling back to X11 (may fail under pure Wayland compositors).\n";
#endif
    }
    (void)resizable;  // not applied on X11 yet (as before)

    // -----------------------------------------------------------------------
    // X connection, HiDPI scale
    // -----------------------------------------------------------------------
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "[Linux] Failed to open X display\n";
        return 1;
    }
    const float x11_scale = getX11DisplayScale(display, DefaultScreen(display));
    if (x11_scale != 1.0f) {
        std::cerr << "[Linux] Display scale: " << x11_scale << "x\n";
    }
    x11::init(display, x11_scale, app_id);

    // Bind the UI thread before any widget tree mutation.
    ThreadChecker::instance().bindToCurrentThread();

    // -----------------------------------------------------------------------
    // One GPU device for every window, created without a window: each window
    // presents through its own campello_gpu Surface (Phase 17).
    // -----------------------------------------------------------------------
    auto device = GPU::Device::createDefaultDevice(nullptr);
    if (!device || !linux_host::init(device)) {
        std::cerr << "[Linux] Failed to create campello_gpu device\n";
        x11::shutdown();
        XCloseDisplay(display);
        return 1;
    }
    std::cerr << "[Linux] Device: " << device->getName()
              << "  Engine: " << GPU::Device::getEngineVersion() << "\n";

    // The main window's View: its pointer dispatcher, focus and text input.
    // makePrimary() also installs them as the process-wide fallbacks, so
    // code without a window context behaves as before. Declared before the
    // window so it is destroyed after it.
    auto view = std::make_unique<View>();
    view->makePrimary();

    // Secondary windows (WindowController / Window widget). Installed before
    // the tree mounts, since a State's initState() may open one.
    auto windowing = x11::makePlatformWindowing();
    PlatformWindowing::setInstance(windowing.get());

    auto ticker = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(ticker.get());

    x11::WindowConfig config;
    config.title  = title;
    config.size   = Size{ static_cast<float>(width), static_cast<float>(height) };
    auto main_window = std::make_unique<x11::X11Window>(*view, config);
    if (!main_window->xid() || !main_window->hasSurface()) {
        std::cerr << "[Linux] Failed to create the main window (no X window or Vulkan surface)\n";
        main_window.reset();
        PlatformWindowing::setInstance(nullptr);
        TickerScheduler::setActive(nullptr);
        linux_host::shutdown();
        x11::shutdown();
        XCloseDisplay(display);
        return 1;
    }

    // Cursor changes (MouseRegion, TextField, ...) go to the window whose
    // event or frame is being handled; the main window otherwise.
    registerCursorHandler([&main_window](SystemMouseCursor c) {
        auto* window = dynamic_cast<x11::X11Window*>(linux_host::LinuxWindow::forView(View::current()));
        (window ? window : main_window.get())->setCursor(c);
    });

    // -----------------------------------------------------------------------
    // MediaQuery for the main window
    // -----------------------------------------------------------------------
    MediaQueryData media;
    media.device_pixel_ratio  = x11_scale;
    media.platform_brightness = getSystemBrightness();
    media.logical_size        = Size{ static_cast<float>(width), static_cast<float>(height) };

    std::shared_ptr<Element> root_element;
    auto rebuildMediaQuery = [&] {
        if (!root_element) return;
        View::Scope scope(view.get());
        root_element->update(std::make_shared<MediaQuery>(media, root_widget));
    };

    if (!initializeDarkModeMonitor([&](Brightness brightness) {
            if (media.platform_brightness == brightness) return;
            media.platform_brightness = brightness;
            std::cerr << "[Linux] platform brightness changed to "
                      << (brightness == Brightness::dark ? "dark" : "light") << "\n";
            rebuildMediaQuery();
        })) {
        std::cerr << "[Linux] xdg-desktop-portal not available; dark-mode live updates disabled.\n";
        // Non-fatal — startup detection still works
    }

    // -----------------------------------------------------------------------
    // IBus IME: one input context, fed by the window with keyboard focus.
    // -----------------------------------------------------------------------
    auto ibus_ime = std::make_unique<IbusIme>();
    if (!ibus_ime->create()) {
        std::cerr << "[Linux] IBus not available; IME composition disabled.\n";
        // Non-fatal — app works without IME
    }
    // Each window wires its own text input to it (see X11Window).
    x11::setIme(ibus_ime.get());

    campello_widgets_initialize_linux_menu_delegate();

    // -----------------------------------------------------------------------
    // Mount the tree (it belongs to the main window)
    // -----------------------------------------------------------------------
    root_element = std::make_shared<MediaQuery>(media, root_widget)->createElement();
    {
        View::Scope scope(view.get());
        root_element->mount(nullptr);
    }
    auto* roe = root_element->findDescendantRenderObjectElement();
    auto render_box = roe ? std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject()) : nullptr;
    if (!render_box) {
        std::cerr << "[Linux] Widget tree produced no root RenderBox\n";
        return 1;
    }
    main_window->setRoot(render_box, Color::white());

    main_window->on_metrics_changed = [&] {
        MediaQueryData next = media;
        next.device_pixel_ratio = main_window->devicePixelRatio();
        next.logical_size       = main_window->logicalSize();
        if (next != media) { media = next; rebuildMediaQuery(); }
    };

    // Closing the main window: by default the app quits, closing every
    // window; with MainWindowClose::keep_running it hides, and the app quits
    // once no other window is left on screen.
    bool running     = true;
    bool main_hidden = false;
    main_window->on_close = [&] {
        if (WindowController::mainWindowClosePolicy() == WindowController::MainWindowClose::quit_app) {
            running = false;
            return;
        }
        main_window->hide();
        main_hidden = true;
    };
    main_window->show();

    // Ctrl+C / kill: leave the loop and tear down normally.
    static volatile sig_atomic_t s_interrupted = 0;
    s_interrupted = 0;
    struct sigaction sa = {};
    sa.sa_handler = [](int) { s_interrupted = 1; linux_host::wake(); };
    sigemptyset(&sa.sa_mask);
    struct sigaction old_int = {}, old_term = {};
    sigaction(SIGINT,  &sa, &old_int);
    sigaction(SIGTERM, &sa, &old_term);

    // -----------------------------------------------------------------------
    // Event loop. poll() blocks on the X connection and the host's wake-up
    // fd (tasks, frame requests); the timeout keeps D-Bus (dark mode, IBus)
    // serviced while idle. A window draws only when it asked for a frame
    // and is mapped.
    // -----------------------------------------------------------------------
    const int x_fd = ConnectionNumber(display);
    while (running && !s_interrupted) {
        while (running && XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);
            if (auto* window = x11::X11Window::forXid(ev.xany.window))
                window->handleEvent(ev);
        }
        if (!running) break;

        pumpDarkModeEvents();
        if (ibus_ime->isActive()) ibus_ime->dispatchEvents();

        linux_host::runTasks();
        if (main_hidden && !x11::X11Window::anyOtherShown(main_window.get())) break;
        linux_host::LinuxWindow::drawPendingFrames();

        if (XPending(display)) continue;
        pollfd fds[2] = {
            { x_fd, POLLIN, 0 },
            { linux_host::wakeFd(), POLLIN, 0 },
        };
        // Until the next paced frame is due; at most 16 ms, which keeps
        // D-Bus (dark mode, IBus) serviced while idle.
        const int next_frame = linux_host::LinuxWindow::msUntilNextFrame();
        const int timeout_ms = next_frame < 0 ? 16 : std::min(next_frame, 16);
        if (poll(fds, 2, timeout_ms) > 0 && (fds[1].revents & POLLIN))
            linux_host::clearWake();
    }

    // -----------------------------------------------------------------------
    // Teardown, in dependency order
    // -----------------------------------------------------------------------
    sigaction(SIGINT,  &old_int, nullptr);
    sigaction(SIGTERM, &old_term, nullptr);
    shutdownDarkModeMonitor();

    // Close every other window first, while the tree is intact: their
    // on_destroyed callbacks usually point into States (setState to drop the
    // controller), which must still exist when those run.
    for (View* v : View::all())
        if (WindowController* controller = WindowController::forView(v)) controller->destroy();
    linux_host::runTasks();

    x11::setIme(nullptr);
    ibus_ime.reset();
    {
        // The tree first, while the managers it unregisters from still exist.
        View::Scope scope(view.get());
        root_element->unmount();
    }
    linux_host::runTasks();
    root_element.reset();
    render_box.reset();
    linux_host::runTasks();

    // Drop the handler before the windows and cursors it reaches.
    registerCursorHandler(nullptr);
    main_window.reset();  // raster thread, renderer, surface, X window (before its View)
    PlatformWindowing::setInstance(nullptr);
    windowing.reset();
    TickerScheduler::setActive(nullptr);
    ticker.reset();

    // Textures cached by images hold the device: release them while it lives.
    ImageCache::instance().clear();

    linux_host::shutdown();
    view.reset();
    device.reset();

    x11::shutdown();
    XCloseDisplay(display);
    return 0;
}

} // namespace systems::leal::campello_widgets
