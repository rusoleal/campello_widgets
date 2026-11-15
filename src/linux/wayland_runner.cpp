#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND

// -----------------------------------------------------------------------------
// Wayland runApp (Phase 17, multi-window)
// -----------------------------------------------------------------------------
// The main window and the event loop. The process-level Wayland state and
// the windows themselves live in wayland_window.{hpp,cpp}; windows opened
// through WindowController come from wayland_platform_windowing.cpp. Every
// window presents through its own campello_gpu Surface on one shared,
// windowless device.
//
// Requires: wayland-client >= 1.20, xkbcommon, libdecor
// -----------------------------------------------------------------------------

#include <campello_widgets/linux/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

#include <campello_gpu/device.hpp>

#include "ibus_ime.hpp"
#include "linux_host.hpp"
#include "wayland_window.hpp"

#include <wayland-client.h>

// C interface installed by src/linux/platform_menu_delegate.cpp — must run
// before the widget tree mounts so PlatformMenuBar::build()'s setMenus()
// and PlatformMenuBarView's needsInWindowMenuBar() check reach the real
// LinuxPlatformMenuDelegate instead of the default no-op.
extern "C" void campello_widgets_initialize_linux_menu_delegate();

#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

// Crash handler: prints a backtrace to stderr on SIGSEGV/SIGABRT.
static void crashHandler(int sig)
{
    void* frames[64];
    int   n = backtrace(frames, 64);
    const char* signame = (sig == SIGSEGV) ? "SIGSEGV" : "SIGABRT";
    // backtrace_symbols_fd and dprintf to a raw fd are safe enough here.
    dprintf(STDERR_FILENO, "\n[CRASH] %s — backtrace (%d frames):\n", signame, n);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    dprintf(STDERR_FILENO, "[CRASH] Tip: addr2line -e <executable> <addr>\n");
    signal(sig, SIG_DFL);
    raise(sig);
}

namespace GPU = ::systems::leal::campello_gpu;

namespace systems::leal::campello_widgets
{

int runAppWayland(const std::string& title, int width, int height,
                  WidgetRef root_widget, bool resizable, const std::string& app_id)
{
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);

    wl_display* display = wl_display_connect(nullptr);
    if (!display) {
        std::cerr << "[Linux/Wayland] Failed to connect to Wayland display\n";
        return 1;
    }
    if (!wayland::init(display, app_id)) {
        wl_display_disconnect(display);
        return 1;
    }

    // Bind the UI thread before any widget tree mutation.
    ThreadChecker::instance().bindToCurrentThread();

    // One GPU device for every window, created without a window.
    auto device = GPU::Device::createDefaultDevice(nullptr);
    if (!device || !linux_host::init(device)) {
        std::cerr << "[Linux/Wayland] Failed to create a campello_gpu device; will fall back to X11.\n";
        device.reset();
        wayland::shutdown();
        wl_display_disconnect(display);
        return 2;  // the caller retries with X11
    }

    // The main window's View; makePrimary() also installs its managers as
    // the process-wide fallbacks. Declared before the window (outlives it).
    auto view = std::make_unique<View>();
    view->makePrimary();

    wayland::WindowConfig config;
    config.title     = title;
    config.size      = Size{ static_cast<float>(width), static_cast<float>(height) };
    config.resizable = resizable;
    auto main_window = std::make_unique<wayland::WaylandWindow>(*view, config);

    auto failToX11 = [&] {
        main_window.reset();
        view.reset();
        linux_host::shutdown();
        device.reset();
        wayland::shutdown();
        wl_display_disconnect(display);
        return 2;
    };
    if (!main_window->valid()) {
        std::cerr << "[Linux/Wayland] Failed to create a Vulkan surface; will fall back to X11.\n";
        return failToX11();
    }

    // Map it and wait for the first configure. One more roundtrip brings
    // wl_surface.enter (the output it appeared on), which sets its scale
    // before anything is drawn.
    main_window->show();
    while (!main_window->everConfigured()) {
        if (!wayland::dispatchWait(16)) {
            std::cerr << "[Linux/Wayland] Connection lost before the window was configured\n";
            return failToX11();
        }
    }
    wl_display_roundtrip(display);
    wayland::dispatch();
    const int scale = static_cast<int>(main_window->devicePixelRatio());
    if (scale != 1) std::cerr << "[Linux/Wayland] Output scale: " << scale << "x\n";
    std::cerr << "[Linux/Wayland] Device: " << device->getName()
              << "  Engine: " << GPU::Device::getEngineVersion() << "\n";

    // Cursor shapes (MouseRegion, TextField, ...) go to the window under
    // the pointer.
    wayland::initCursor(scale);
    registerCursorHandler([](SystemMouseCursor c) {
        for (linux_host::LinuxWindow* w : linux_host::LinuxWindow::all())
            static_cast<wayland::WaylandWindow*>(w)->setCursor(c);  // only the one under the pointer acts
    });

    // Secondary windows (WindowController / Window widget). Installed before
    // the tree mounts, since a State's initState() may open one.
    auto windowing = wayland::makePlatformWindowing();
    PlatformWindowing::setInstance(windowing.get());

    auto ticker = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(ticker.get());

    // IBus: one input context, fed by the window with keyboard focus.
    auto ibus_ime = std::make_unique<IbusIme>();
    if (!ibus_ime->create())
        std::cerr << "[Linux/Wayland] IBus not available; IME composition disabled.\n";
    wayland::setIme(ibus_ime.get());

    campello_widgets_initialize_linux_menu_delegate();

    // -------------------------------------------------------------------------
    // Mount the tree (it belongs to the main window)
    // -------------------------------------------------------------------------
    MediaQueryData media;
    media.device_pixel_ratio = main_window->devicePixelRatio();
    media.logical_size       = main_window->logicalSize();

    std::shared_ptr<Element> root_element = std::make_shared<MediaQuery>(media, root_widget)->createElement();
    {
        View::Scope view_scope(view.get());
        root_element->mount(nullptr);
    }
    auto* roe = root_element->findDescendantRenderObjectElement();
    auto render_box = roe ? std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject()) : nullptr;
    if (!render_box) {
        std::cerr << "[Linux/Wayland] Widget tree produced no root RenderBox\n";
        return 1;
    }
    main_window->setRoot(render_box, Color::white());

    // A resize (or scale change) updates the MediaQuery the tree sees.
    main_window->on_metrics_changed = [&] {
        MediaQueryData next = media;
        next.device_pixel_ratio = main_window->devicePixelRatio();
        next.logical_size       = main_window->logicalSize();
        if (next == media) return;
        media = next;
        View::Scope view_scope(view.get());
        root_element->update(std::make_shared<MediaQuery>(media, root_widget));
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

    // Ctrl+C / kill: leave the loop and tear down normally.
    static volatile sig_atomic_t s_interrupted = 0;
    s_interrupted = 0;
    struct sigaction sa = {};
    sa.sa_handler = [](int) { s_interrupted = 1; linux_host::wake(); };
    sigemptyset(&sa.sa_mask);
    struct sigaction old_int = {}, old_term = {};
    sigaction(SIGINT,  &sa, &old_int);
    sigaction(SIGTERM, &sa, &old_term);

    // Test hook: only the compositor can close a Wayland window, so with
    // CAMPELLO_WIDGETS_SIGUSR1_CLOSE=1, SIGUSR1 acts as the main window's
    // close button (the multi-window example's self-tests use it).
    static volatile sig_atomic_t s_close_requested = 0;
    s_close_requested = 0;
    struct sigaction old_usr1 = {};
    const char* close_hook = std::getenv("CAMPELLO_WIDGETS_SIGUSR1_CLOSE");
    const bool  use_close_hook = close_hook && close_hook[0] == '1';
    if (use_close_hook) {
        struct sigaction usr1 = {};
        usr1.sa_handler = [](int) { s_close_requested = 1; linux_host::wake(); };
        sigemptyset(&usr1.sa_mask);
        sigaction(SIGUSR1, &usr1, &old_usr1);
    }

    std::cerr << "[Linux/Wayland] Renderer ready, entering event loop\n";

    // -------------------------------------------------------------------------
    // Event loop. poll() blocks on the Wayland connection and the host's
    // wake-up fd (tasks, frame requests); frames are paced by each window's
    // frame callbacks. The timeout keeps IBus serviced while idle.
    // -------------------------------------------------------------------------
    while (running && !s_interrupted) {
        wayland::dispatch();

        if (ibus_ime->isActive()) {
            // Committed text goes to the window with keyboard focus.
            linux_host::LinuxWindow* focused = nullptr;
            for (linux_host::LinuxWindow* w : linux_host::LinuxWindow::all())
                if (static_cast<wayland::WaylandWindow*>(w)->hasKeyboardFocus()) focused = w;
            View::Scope ime_scope(focused ? &focused->view() : view.get());
            ibus_ime->dispatchEvents();
        }

        if (s_close_requested) {
            s_close_requested = 0;
            if (!main_hidden) main_window->requestCloseAsync();
        }
        linux_host::runTasks();
        if (!running) break;
        if (main_hidden && !wayland::WaylandWindow::anyOtherShown(main_window.get())) break;
        linux_host::LinuxWindow::drawPendingFrames();
        wl_display_flush(display);

        if (wl_display_prepare_read(display) != 0) continue;  // events already queued
        pollfd fds[2] = {
            { wl_display_get_fd(display), POLLIN, 0 },
            { linux_host::wakeFd(), POLLIN, 0 },
        };
        const int timeout_ms = linux_host::LinuxWindow::anyFramePending() ? 0 : 16;
        const int ret = poll(fds, 2, timeout_ms);
        if (ret > 0 && (fds[0].revents & POLLIN)) wl_display_read_events(display);
        else                                       wl_display_cancel_read(display);
        if (ret > 0 && (fds[1].revents & POLLIN)) linux_host::clearWake();
    }

    // -------------------------------------------------------------------------
    // Teardown, in dependency order
    // -------------------------------------------------------------------------
    sigaction(SIGINT,  &old_int, nullptr);
    sigaction(SIGTERM, &old_term, nullptr);
    if (use_close_hook) sigaction(SIGUSR1, &old_usr1, nullptr);

    // Close every other window first, while the tree is intact: their
    // on_destroyed callbacks usually point into States.
    for (View* v : View::all())
        if (WindowController* controller = WindowController::forView(v)) controller->destroy();
    linux_host::runTasks();

    wayland::setIme(nullptr);
    ibus_ime.reset();
    main_window->on_metrics_changed = nullptr;
    {
        // The tree first, while the managers it unregisters from still exist.
        View::Scope view_scope(view.get());
        root_element->unmount();
    }
    linux_host::runTasks();
    root_element.reset();
    render_box.reset();
    linux_host::runTasks();

    registerCursorHandler(nullptr);
    // Raster thread, renderer and Surface, then the wl_surface (before the View).
    main_window.reset();
    PlatformWindowing::setInstance(nullptr);
    windowing.reset();
    TickerScheduler::setActive(nullptr);
    ticker.reset();

    // Textures cached by images hold the device: release them while it lives.
    ImageCache::instance().clear();

    // The device while the display is still open: on hasvk/Wayland,
    // vkDestroySurfaceKHR talks to the compositor.
    linux_host::shutdown();
    view.reset();
    device.reset();

    wayland::shutdown();
    wl_display_disconnect(display);
    return 0;
}

} // namespace systems::leal::campello_widgets

#endif // CAMPELLO_WIDGETS_HAS_WAYLAND
