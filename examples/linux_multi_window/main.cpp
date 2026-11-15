// Multi-window example for Linux / X11 (Phase 17): one app, one widget tree,
// several native windows.
//
// The main window opens an "Inspector" (a regular window), an "About"
// dialog, a modal "Preferences" window (the main window takes no input while
// it is open) and an "Options" popup menu, a borderless window that extends
// past the main window's edge. All of them show and change the same counter,
// which lives in the app's State: windows share state through the one element
// tree, with no messaging. The inspector also has a TextField, to show that
// each window has its own focus and text input.
//
// CW_MULTIWINDOW_DEMO=1  opens the inspector and the About dialog at launch.
// CW_MULTIWINDOW_DEMO=2  self-test: shared counter, per-window text input,
//                        closing a dialog.
// CW_MULTIWINDOW_DEMO=3  self-test: modal window, popup menu, main-window
//                        close policy (quit).
// CW_MULTIWINDOW_DEMO=4  self-test: MainWindowClose::keep_running.
//
// The self-tests send real X input through the X server's XTest extension
// (pointer moves, button and key presses), so events reach each window the
// way a user's do, print what they observe, and, if CW_MULTIWINDOW_DEMO_DIR is
// set, save what each window shows (XGetImage) as <dir>/<step>_<title>.ppm.
// Without a window manager (e.g. on Xvfb) nothing moves keyboard focus when a
// window is clicked; the tests then focus windows themselves (focusWindow()),
// as a window manager would on a click.
//
// Under Wayland (WAYLAND_DISPLAY set) a client can't synthesise input or read
// other surfaces, so the self-tests inject pointer and key events straight
// into each window's View, close the main window through the runner's
// SIGUSR1 test hook (CAMPELLO_WIDGETS_SIGUSR1_CLOSE, set by main()), and save
// each window's next frame (Renderer::requestFrameCapture) as a PNG. The
// windows themselves (toplevels, dialogs, the xdg_popup menu) are real.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/linux/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace cw = systems::leal::campello_widgets;

namespace
{
    cw::TextStyle style(float size, cw::Color color)
    {
        cw::TextStyle s{};
        s.font_size   = size;
        s.color       = color;
        return s;
    }

    const cw::Color kInk    = cw::Color::fromRGB(0.15f, 0.15f, 0.18f);
    const cw::Color kMuted  = cw::Color::fromRGB(0.45f, 0.45f, 0.50f);
    const cw::Color kAccent = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);

    // A simple pressable label.
    cw::WidgetRef button(const std::string& label, cw::Color color, std::function<void()> on_press)
    {
        auto box    = std::make_shared<cw::Container>();
        box->color  = color;
        box->height = 36.0f;
        box->child  = cw::mw<cw::Center>(cw::mw<cw::Padding>(
            cw::EdgeInsets::symmetric(0.0f, 16.0f),
            cw::mw<cw::Text>(label, style(14.0f, cw::Color::white()))));

        auto tap    = std::make_shared<cw::GestureDetector>();
        tap->on_tap = std::move(on_press);
        tap->child  = box;

        auto cursor    = std::make_shared<cw::MouseRegion>();
        cursor->cursor = cw::SystemMouseCursor::pointer;
        cursor->child  = tap;
        return cw::mw<cw::Padding>(cw::EdgeInsets::all(6.0f), cursor);
    }

    // A window's page: a title, the shared counter, and some actions.
    cw::WidgetRef page(const std::string& title, int count, std::vector<cw::WidgetRef> actions,
                       cw::Color background)
    {
        std::vector<cw::WidgetRef> rows{
            cw::mw<cw::Text>(title, style(20.0f, kInk)),
            cw::mw<cw::SizedBox>(std::optional<float>{}, std::optional<float>{12.0f}),
            cw::mw<cw::Text>("shared counter", style(13.0f, kMuted)),
            cw::mw<cw::Text>(std::to_string(count), style(64.0f, kAccent)),
            cw::mw<cw::SizedBox>(std::optional<float>{}, std::optional<float>{12.0f}),
        };
        for (auto& a : actions) rows.push_back(a);

        auto bg   = std::make_shared<cw::Container>();
        bg->color = background;
        bg->child = cw::mw<cw::Center>(cw::mw<cw::Column>(
            cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center, rows));
        return bg;
    }


    // ------------------------------------------------------------------
    // Self-test helpers: real X input (XTest) to this process's windows,
    // through a connection of our own.
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // Wayland: events injected into each window's View (see the top).
    // ------------------------------------------------------------------

    bool g_wayland = false;
    // Set by the app's State: the View of the window titled `title` ("" is
    // the popup), nullptr if it isn't open; and the window controller (to
    // close a secondary window as its close button would).
    std::function<cw::View*(const char*)>             g_view_titled;
    std::function<cw::WindowController*(const char*)> g_controller_titled;
    std::function<std::string(cw::View*)>             g_title_of_view;

    void injectPointer(cw::View* view, cw::PointerEventKind kind, float x, float y)
    {
        cw::View::Scope scope(view);
        cw::PointerEvent e;
        e.kind        = kind;
        e.pointer_id  = 0;
        e.position    = {x, y};
        e.pressure    = kind == cw::PointerEventKind::down ? 1.0f : 0.0f;
        e.button      = cw::PointerButton::primary;
        e.device_kind = cw::PointerDeviceKind::mouse;
        view->pointerDispatcher().handlePointerEvent(e);
    }

    void injectKey(cw::View* view, cw::KeyCode code, uint32_t character)
    {
        cw::View::Scope scope(view);
        cw::KeyEvent ke;
        ke.kind      = cw::KeyEventKind::down;
        ke.key_code  = code;
        ke.modifiers = cw::KeyModifiers::none;
        ke.character = character;
        view->focusManager().handleKeyEvent(ke);
        if (!cw::View::isAlive(view)) return;
        ke.kind      = cw::KeyEventKind::up;
        ke.character = 0;
        view->focusManager().handleKeyEvent(ke);
    }

    // Windows that are drawing (a hidden Wayland window stops its renderer).
    std::vector<cw::View*> shownViews()
    {
        std::vector<cw::View*> out;
        for (cw::View* v : cw::View::all())
            if (v->renderer()) out.push_back(v);
        return out;
    }

    Display* dpy()
    {
        static Display* d = XOpenDisplay(nullptr);
        return d;
    }

    Atom atom(const char* name) { return XInternAtom(dpy(), name, False); }

    // XTest, loaded at run time (libXtst's headers aren't a build dependency).
    struct XTest
    {
        int (*motion)(Display*, int, int, int, unsigned long)      = nullptr;
        int (*button)(Display*, unsigned int, Bool, unsigned long) = nullptr;
        int (*key)(Display*, unsigned int, Bool, unsigned long)    = nullptr;
    };
    const XTest& xtest()
    {
        static XTest t = [] {
            XTest x;
            if (void* lib = dlopen("libXtst.so.6", RTLD_LAZY))
            {
                x.motion = reinterpret_cast<decltype(x.motion)>(dlsym(lib, "XTestFakeMotionEvent"));
                x.button = reinterpret_cast<decltype(x.button)>(dlsym(lib, "XTestFakeButtonEvent"));
                x.key    = reinterpret_cast<decltype(x.key)>(dlsym(lib, "XTestFakeKeyEvent"));
            }
            if (!x.motion || !x.button || !x.key) std::printf("[demo] XTest (libXtst.so.6) not available\n");
            return x;
        }();
        return t;
    }

    std::string titleOf(::Window w)
    {
        Atom type; int format; unsigned long n = 0, after = 0; unsigned char* data = nullptr;
        std::string title;
        if (XGetWindowProperty(dpy(), w, atom("_NET_WM_NAME"), 0, 256, False, atom("UTF8_STRING"),
                               &type, &format, &n, &after, &data) == Success && data)
        {
            title.assign(reinterpret_cast<char*>(data), n);
            XFree(data);
        }
        return title;
    }

    long pidOf(::Window w)
    {
        Atom type; int format; unsigned long n = 0, after = 0; unsigned char* data = nullptr;
        long pid = 0;
        if (XGetWindowProperty(dpy(), w, atom("_NET_WM_PID"), 0, 1, False, XA_CARDINAL,
                               &type, &format, &n, &after, &data) == Success && data)
        {
            if (n == 1) pid = *reinterpret_cast<long*>(data);
            XFree(data);
        }
        return pid;
    }

    bool isPopup(::Window w)
    {
        XWindowAttributes a{};
        return XGetWindowAttributes(dpy(), w, &a) && a.override_redirect;
    }

    bool isViewable(::Window w)
    {
        XWindowAttributes a{};
        return XGetWindowAttributes(dpy(), w, &a) && a.map_state == IsViewable;
    }

    // This process's windows (a window manager reparents clients into
    // frames, so look two levels down from the root).
    std::vector<::Window> ourWindows(bool visible_only = true)
    {
        std::vector<::Window> out;
        const long me = static_cast<long>(getpid());
        std::function<void(::Window, int)> walk = [&](::Window w, int depth) {
            ::Window root, parent, *children = nullptr;
            unsigned int count = 0;
            if (!XQueryTree(dpy(), w, &root, &parent, &children, &count)) return;
            for (unsigned int i = 0; i < count; ++i)
            {
                if (pidOf(children[i]) == me) { if (!visible_only || isViewable(children[i])) out.push_back(children[i]); }
                else if (depth < 2) walk(children[i], depth + 1);
            }
            if (children) XFree(children);
        };
        walk(DefaultRootWindow(dpy()), 0);
        return out;
    }

    // "" names the popup (it has no title).
    ::Window windowTitled(const char* title)
    {
        for (::Window w : ourWindows())
            if (*title == '\0' ? isPopup(w) : (!isPopup(w) && titleOf(w) == title)) return w;
        return 0;
    }

    struct Geometry { int x = 0, y = 0, w = 0, h = 0; };
    Geometry geometryOf(::Window w)
    {
        Geometry g;
        XWindowAttributes a{};
        if (!XGetWindowAttributes(dpy(), w, &a)) return g;
        ::Window child;
        XTranslateCoordinates(dpy(), w, DefaultRootWindow(dpy()), 0, 0, &g.x, &g.y, &child);
        g.w = a.width;
        g.h = a.height;
        return g;
    }

    std::string focusedTitle()
    {
        if (g_wayland) return "n/a";  // not visible to a Wayland client
        ::Window focus = 0; int revert = 0;
        XGetInputFocus(dpy(), &focus, &revert);
        if (!focus || focus == PointerRoot) return "(none)";
        return isPopup(focus) ? "(popup)" : titleOf(focus);
    }

    float dpr()
    {
        cw::View* v = cw::View::primary();
        return v && v->renderer() ? v->renderer()->devicePixelRatio() : 1.0f;
    }

    // A click at (x, y): logical pixels from the window content's top-left.
    void clickInWindow(const char* title, float x, float y)
    {
        if (g_wayland)
        {
            cw::View* v = g_view_titled(title);
            if (!v) { std::printf("[demo] no window titled '%s'\n", title); return; }
            injectPointer(v, cw::PointerEventKind::move, x, y);
            if (cw::View::isAlive(v)) injectPointer(v, cw::PointerEventKind::down, x, y);
            if (cw::View::isAlive(v)) injectPointer(v, cw::PointerEventKind::up, x, y);
            std::printf("[demo] clicked '%s' at (%.0f, %.0f)\n", *title ? title : "(popup)", x, y);
            return;
        }
        const ::Window w = windowTitled(title);
        if (!w || !xtest().motion) { std::printf("[demo] no window titled '%s'\n", title); return; }
        int rx = 0, ry = 0;
        ::Window child;
        XTranslateCoordinates(dpy(), w, DefaultRootWindow(dpy()),
                              static_cast<int>(x * dpr()), static_cast<int>(y * dpr()), &rx, &ry, &child);
        xtest().motion(dpy(), -1, rx, ry, CurrentTime);
        xtest().button(dpy(), Button1, True, CurrentTime);
        xtest().button(dpy(), Button1, False, CurrentTime);
        XFlush(dpy());
        std::printf("[demo] clicked '%s' at (%.0f, %.0f)\n", *title ? title : "(popup)", x, y);
    }

    // Gives `title` keyboard focus, as a window manager does on a click.
    void focusWindow(const char* title)
    {
        if (g_wayland) return;  // keys are injected into the window's View
        const ::Window w = windowTitled(title);
        if (!w) { std::printf("[demo] no window titled '%s'\n", title); return; }
        XSetInputFocus(dpy(), w, RevertToParent, CurrentTime);
        XFlush(dpy());
        std::printf("[demo] focused '%s'\n", title);
    }

    void pressKey(KeySym keysym)
    {
        const KeyCode code = XKeysymToKeycode(dpy(), keysym);
        xtest().key(dpy(), code, True, CurrentTime);
        xtest().key(dpy(), code, False, CurrentTime);
    }

    // Keys go to the focused window: focus `title` first (see focusWindow()).
    void typeInWindow(const char* title, const char* text)
    {
        if (g_wayland)
        {
            cw::View* v = g_view_titled(title);
            if (!v) { std::printf("[demo] no window titled '%s'\n", title); return; }
            for (const char* c = text; *c && cw::View::isAlive(v); ++c)
                injectKey(v, cw::KeyCode::unknown, static_cast<unsigned char>(*c));
            std::printf("[demo] typed '%s' into '%s'\n", text, title);
            return;
        }
        if (!xtest().key) return;
        for (const char* c = text; *c; ++c) pressKey(static_cast<KeySym>(*c));
        XFlush(dpy());
        std::printf("[demo] typed '%s' (focus: '%s', meant for '%s')\n", text, focusedTitle().c_str(), title);
    }

    void pressEscape(const char* title)
    {
        if (g_wayland)
        {
            cw::View* v = g_view_titled(title);
            if (!v) { std::printf("[demo] no window titled '%s'\n", *title ? title : "(popup)"); return; }
            injectKey(v, cw::KeyCode::escape, 0);
            std::printf("[demo] pressed Escape in '%s'\n", *title ? title : "(popup)");
            return;
        }
        if (!xtest().key) return;
        pressKey(XK_Escape);
        XFlush(dpy());
        std::printf("[demo] pressed Escape (focus: '%s', meant for '%s')\n", focusedTitle().c_str(),
                    *title ? title : "(popup)");
    }

    // The close button: WM_DELETE_WINDOW, as a window manager sends it.
    void closeWindow(const char* title)
    {
        if (g_wayland)
        {
            std::printf("[demo] closing '%s'\n", title);
            if (cw::WindowController* c = g_controller_titled(title)) c->requestClose();
            else if (g_view_titled(title) == cw::View::primary()) raise(SIGUSR1);  // the runner's test hook
            else std::printf("[demo] no window titled '%s' to close\n", title);
            return;
        }
        const ::Window w = windowTitled(title);
        if (!w) { std::printf("[demo] no window titled '%s' to close\n", title); return; }
        XEvent ev{};
        ev.xclient.type         = ClientMessage;
        ev.xclient.window       = w;
        ev.xclient.message_type = atom("WM_PROTOCOLS");
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = static_cast<long>(atom("WM_DELETE_WINDOW"));
        ev.xclient.data.l[1]    = CurrentTime;
        XSendEvent(dpy(), w, False, NoEventMask, &ev);
        XFlush(dpy());
        std::printf("[demo] closing '%s'\n", title);
    }

    // What the window shows, as a binary PPM.
    void saveWindowImage(::Window w, const std::string& path)
    {
        XWindowAttributes a{};
        if (!XGetWindowAttributes(dpy(), w, &a) || a.width <= 0 || a.height <= 0) return;
        XImage* img = XGetImage(dpy(), w, 0, 0, a.width, a.height, AllPlanes, ZPixmap);
        if (!img) return;
        if (FILE* f = std::fopen(path.c_str(), "wb"))
        {
            std::fprintf(f, "P6\n%d %d\n255\n", a.width, a.height);
            for (int y = 0; y < a.height; ++y)
                for (int x = 0; x < a.width; ++x)
                {
                    const unsigned long p = XGetPixel(img, x, y);
                    const unsigned char rgb[3] = {static_cast<unsigned char>((p >> 16) & 0xFF),
                                                  static_cast<unsigned char>((p >> 8) & 0xFF),
                                                  static_cast<unsigned char>(p & 0xFF)};
                    std::fwrite(rgb, 1, 3, f);
                }
            std::fclose(f);
        }
        XDestroyImage(img);
    }

    void captureAllWindows(const char* step)
    {
        const char* dir = std::getenv("CW_MULTIWINDOW_DEMO_DIR");
        if (!dir) return;
        if (const char* cmd = std::getenv("CW_MULTIWINDOW_SCREENSHOT"))
        {
            // The whole screen, by the compositor (e.g. weston-screenshooter
            // on a headless Weston): where the windows are and how they
            // stack. `{step}` in the command becomes the step's name. Taken
            // a moment later, once the step's frames are on screen.
            std::string command = cmd;
            if (const auto at = command.find("{step}"); at != std::string::npos) command.replace(at, 6, step);
            std::thread([command] {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                if (std::system(command.c_str()) != 0) std::printf("[demo] screenshot command failed\n");
            }).detach();
        }
        if (g_wayland)
        {
            // Each window's next frame, as its renderer draws it.
            for (cw::View* v : shownViews())
            {
                std::string name = g_title_of_view(v);
                for (char& c : name) if (c == ' ' || c == '-') c = '_';
                v->renderer()->requestFrameCapture(std::string(dir) + "/" + step + "_" + name + ".png");
                v->renderer()->notePaintRequested();
                v->requestFrame();
            }
            return;
        }
        for (::Window w : ourWindows())
        {
            std::string name = isPopup(w) ? "popup" : titleOf(w);
            for (char& c : name) if (c == ' ' || c == '-') c = '_';
            saveWindowImage(w, std::string(dir) + "/" + step + "_" + name + ".ppm");
        }
    }

    // Runs `steps` one after another, `interval_ms` apart, on the UI thread:
    // a helper thread waits and posts each step (the Linux host's postTask()
    // may be called from any thread).
    void runScript(std::vector<std::function<void()>> steps, unsigned interval_ms)
    {
        std::thread([steps = std::move(steps), interval_ms] {
            for (const auto& step : steps)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                auto* windowing = cw::PlatformWindowing::instance();
                if (!windowing) return;  // the app is shutting down
                windowing->postTask([step] { step(); std::fflush(stdout); });
            }
        }).detach();
    }
}

class MultiWindowApp;

class MultiWindowState : public cw::State<MultiWindowApp>
{
public:
    void initState() override
    {
        g_view_titled = [this](const char* title) -> cw::View* {
            const std::string t = title;
            if (t == "Multi-window") return cw::View::primary();
            if (t.empty())
            {
                for (cw::View* v : cw::View::all())
                    if (auto* c = cw::WindowController::forView(v); c && c->kind() == cw::WindowKind::popup) return v;
                return nullptr;
            }
            cw::WindowController* c = g_controller_titled(title);
            return c && c->platformWindow() ? &c->platformWindow()->view() : nullptr;
        };
        g_controller_titled = [this](const char* title) -> cw::WindowController* {
            const std::string t = title;
            if (t == "Inspector")   return inspector_.get();
            if (t == "About")       return about_.get();
            if (t == "Preferences") return prefs_.get();
            return nullptr;
        };
        g_title_of_view = [this](cw::View* v) -> std::string {
            if (v == cw::View::primary()) return "Multi-window";
            for (const char* t : {"Inspector", "About", "Preferences"})
                if (g_view_titled(t) == v) return t;
            return "popup";
        };

        const char* demo = std::getenv("CW_MULTIWINDOW_DEMO");
        const char  mode = demo ? *demo : '0';
        if (mode == '1' || mode == '2')
        {
            count_ = 3;
            openInspector();
            openAbout();
        }
        if (mode == '2') runSelfTest();
        if (mode == '3') { count_ = 3; runModalAndPopupTest(); }
        if (mode == '4') { count_ = 3; runKeepRunningTest(); }
    }

    void dispose() override
    {
        // Close our windows with us. destroy() completes on a later turn,
        // after this State is gone: drop the callbacks that point at it.
        for (auto* window : {&inspector_, &about_, &prefs_})
        {
            if (!*window) continue;
            (*window)->on_destroyed = nullptr;
            (*window)->destroy();
        }
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto increment = [this] { setState([this] { ++count_; }); };

        std::vector<cw::WidgetRef> actions{
            button("+1", kAccent, increment),
            button(inspector_ ? "Inspector is open" : "Open inspector", cw::Color::fromRGB(0.2f, 0.6f, 0.35f),
                   [this] { if (!inspector_) openInspector(); else inspector_->show(); }),
            button("About...", cw::Color::fromRGB(0.5f, 0.5f, 0.55f),
                   [this] { if (!about_) openAbout(); else about_->show(); }),
            button("Preferences... (modal)", cw::Color::fromRGB(0.55f, 0.3f, 0.7f),
                   [this] { if (!prefs_) openPreferences(); }),
            optionsMenu(increment),
        };

        // The secondary windows' content lives right here in the main tree;
        // each Window widget is an empty leaf in this window.
        if (inspector_)
        {
            auto field = std::make_shared<cw::TextField>();
            field->controller = field_;
            actions.push_back(std::make_shared<cw::Window>(inspector_, page(
                "Inspector", count_,
                {button("+1 from the inspector", kAccent, increment),
                 cw::mw<cw::SizedBox>(std::optional<float>{220.0f}, std::optional<float>{40.0f}, field)},
                cw::Color::fromRGB(0.93f, 0.97f, 0.94f))));
        }
        if (about_)
        {
            actions.push_back(std::make_shared<cw::Window>(about_, page(
                "About", count_,
                {button("Close", cw::Color::fromRGB(0.85f, 0.2f, 0.15f), [this] { about_->destroy(); })},
                cw::Color::fromRGB(0.97f, 0.95f, 0.92f))));
        }
        if (prefs_)
        {
            actions.push_back(std::make_shared<cw::Window>(prefs_, page(
                "Preferences", count_,
                {button("+10", kAccent, [this] { setState([this] { count_ += 10; }); }),
                 button("Done", cw::Color::fromRGB(0.2f, 0.6f, 0.35f), [this] { prefs_->destroy(); })},
                cw::Color::fromRGB(0.95f, 0.93f, 0.98f))));
        }

        return page("Main window", count_, actions, cw::Color::fromRGB(0.97f, 0.97f, 0.98f));
    }

    // "Options v" and its popup menu (a native popup window).
    cw::WidgetRef optionsMenu(std::function<void()> increment)
    {
        auto item = [this](const std::string& label, std::function<void()> action) {
            return button(label, cw::Color::fromRGB(0.25f, 0.25f, 0.3f), [this, action] {
                if (action) action();
                setState([this] { menu_open_ = false; });
            });
        };
        auto menu_bg   = std::make_shared<cw::Container>();
        menu_bg->color = cw::Color::fromRGB(0.16f, 0.16f, 0.19f);
        menu_bg->child = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            std::vector<cw::WidgetRef>{
                item("+1 from the menu", increment),
                item("Reset counter", [this] { setState([this] { count_ = 0; }); }),
                item("Open inspector", [this] { if (!inspector_) openInspector(); }),
            });

        auto anchor        = std::make_shared<cw::PopupWindowAnchor>();
        anchor->child      = button("Options v", cw::Color::fromRGB(0.3f, 0.3f, 0.35f),
                                    [this] { setState([this] { menu_open_ = !menu_open_; }); });
        anchor->popup      = menu_bg;
        anchor->open       = menu_open_;
        anchor->popup_size = {380.0f, 156.0f};
        anchor->on_dismiss = [this] { setState([this] { menu_open_ = false; }); };
        return anchor;
    }

private:
    void report(const char* step)
    {
        const ::Window main  = g_wayland ? 0 : windowTitled("Multi-window");
        const ::Window popup = g_wayland ? 0 : windowTitled("");
        std::printf("[demo] %s: count=%d inspector=%s about=%s prefs=%s menu=%s field='%s' "
                    "windows=%zu focus='%s' main_blocked=%d\n",
                    step, count_, inspector_ ? "open" : "closed", about_ ? "open" : "closed",
                    prefs_ ? "open" : "closed", menu_open_ ? "open" : "closed", field_->text().c_str(),
                    g_wayland ? shownViews().size() : ourWindows().size(), focusedTitle().c_str(),
                    cw::View::primary() && cw::View::primary()->isInputBlocked() ? 1 : 0);
        if (popup && main)
        {
            const Geometry p = geometryOf(popup), m = geometryOf(main);
            std::printf("[demo]   popup y=[%d..%d] main y=[%d..%d] -> extends below main: %s\n",
                        p.y, p.y + p.h, m.y, m.y + m.h, p.y + p.h > m.y + m.h ? "yes" : "no");
        }
    }
    // CW_MULTIWINDOW_DEMO=2. Coordinates are logical pixels from the top-left
    // of each window's client area.
    void runSelfTest()
    {
        runScript({
            [this] { report("start"); captureAllWindows("0_start"); },
            [this] { clickInWindow("Inspector", 180.0f, kInspectorPlusY); },
            [this] { report("clicked +1 in the inspector"); captureAllWindows("1_after_inspector_click"); },
            [this] { clickInWindow("Inspector", 180.0f, kInspectorFieldY); },
            [this] { focusWindow("Inspector"); typeInWindow("Inspector", "hi"); },
            [this] { report("typed into the inspector's field"); captureAllWindows("2_after_typing"); },
            [this] { focusWindow("Multi-window"); typeInWindow("Multi-window", "x"); },  // no text field there
            [this] { report("typed into the main window"); },
            [this] { clickInWindow("About", 150.0f, kAboutCloseY); },
            [this] { report("clicked Close in About"); captureAllWindows("3_after_about_close"); },
            [] { closeWindow("Multi-window"); },
        }, 700);
    }

    // CW_MULTIWINDOW_DEMO=3: modal windows and popup menus.
    void runModalAndPopupTest()
    {
        runScript({
            [this] { report("start"); captureAllWindows("0_layout"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainPrefsY); },
            [this] { report("preferences opened"); captureAllWindows("1_modal"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainPlusY); },  // blocked
            [this] { report("clicked +1 in the blocked main window"); },
            [] { clickInWindow("Preferences", 160.0f, kPrefsPlusTenY); },
            [this] { report("clicked +10 in Preferences"); },
            [] { clickInWindow("Preferences", 160.0f, kPrefsDoneY); },
            [this] { report("preferences closed"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainPlusY); },  // works again
            [this] { report("clicked +1 again"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainOptionsY); },
            [this] { report("menu opened"); captureAllWindows("2_menu"); },
            [] { clickInWindow("", 190.0f, 24.0f); },  // "+1 from the menu"
            [this] { report("menu item chosen"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainOptionsY); },
            [this] { report("menu reopened"); },
            [] { clickInWindow("Multi-window", 260.0f, 60.0f); },  // outside it
            [this] { report("clicked outside the menu"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainOptionsY); },
            [this] { report("menu reopened"); },
            [] { pressEscape(""); },
            [this] { report("pressed Escape"); },
            [] { clickInWindow("Multi-window", 260.0f, kMainOptionsY); },
            [this] { report("menu reopened"); },
            [] { focusWindow("Multi-window"); },  // focus moves away from the popup
            [this] { report("popup lost focus"); },
            [this] { openInspector(); openAbout(); },
            [this] {
                report("before closing the main window");
                std::printf("[demo] closing the main window (default policy: quit)\n");
                closeWindow("Multi-window");
            },
            [] { std::printf("[demo] STILL RUNNING after closing the main window\n"); },
        }, 600);
    }

    // CW_MULTIWINDOW_DEMO=4: with keep_running, closing the main window keeps
    // the others; the app quits when the last one closes.
    void runKeepRunningTest()
    {
        cw::WindowController::setMainWindowClosePolicy(cw::WindowController::MainWindowClose::keep_running);
        runScript({
            [this] { openInspector(); },
            [this] { report("inspector opened"); closeWindow("Multi-window"); },
            [this] { report("main window closed"); clickInWindow("Inspector", 180.0f, kInspectorPlusY); },
            [this] { report("clicked +1 in the inspector"); closeWindow("Inspector"); },
            [] { std::printf("[demo] STILL RUNNING after closing the last window\n"); },
        }, 600);
    }

    void openInspector()
    {
        // Placed beside the main window: without a window manager (Xvfb)
        // every window would open at the screen's top-left, under the others.
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {360.0f, 420.0f},
                                                   .position = cw::Offset{560.0f, 40.0f}});
        if (!inspector_) { std::printf("[demo] could not open the inspector\n"); return; }
        // Closed by the user: drop it so its Window widget leaves the tree.
        inspector_->on_destroyed = [this] { setState([this] { inspector_.reset(); }); };
        setState([] {});
    }

    void openPreferences()
    {
        prefs_ = cw::WindowController::create({.title = "Preferences", .size = {320.0f, 280.0f},
                                               .kind = cw::WindowKind::dialog, .modal = true,
                                               .resizable = false});
        if (!prefs_) return;
        prefs_->on_destroyed = [this] { setState([this] { prefs_.reset(); }); };
        setState([] {});
    }

    void openAbout()
    {
        about_ = cw::WindowController::create({.title = "About", .size = {300.0f, 260.0f},
                                               .kind = cw::WindowKind::dialog, .resizable = false});
        if (!about_) return;
        about_->on_destroyed = [this] { setState([this] { about_.reset(); }); };
        setState([] {});
    }

    // Layout positions (logical pixels), read off captures on Xvfb (scale 1).
    static constexpr float kMainPlusY       = 233.0f;
    static constexpr float kMainPrefsY      = 377.0f;
    static constexpr float kMainOptionsY    = 424.0f;
    static constexpr float kInspectorPlusY  = 240.0f;
    static constexpr float kInspectorFieldY = 286.0f;
    static constexpr float kAboutCloseY     = 179.0f;
    static constexpr float kPrefsPlusTenY   = 165.0f;
    static constexpr float kPrefsDoneY      = 212.0f;

    int                                       count_ = 0;
    std::shared_ptr<cw::TextEditingController> field_ = std::make_shared<cw::TextEditingController>();
    std::shared_ptr<cw::WindowController>     inspector_;
    std::shared_ptr<cw::WindowController>     about_;
    std::shared_ptr<cw::WindowController>     prefs_;
    bool                                      menu_open_ = false;
};

class MultiWindowApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<MultiWindowState>();
    }
};

int main()
{
    // The self-tests' helper thread talks to the X server too.
    XInitThreads();
    const char* wl = std::getenv("WAYLAND_DISPLAY");
    g_wayland = wl && *wl;
    if (g_wayland) setenv("CAMPELLO_WIDGETS_SIGUSR1_CLOSE", "1", 1);
    const int result = cw::runApp("Multi-window", 520, 560, std::make_shared<MultiWindowApp>());
    std::printf("[demo] runApp returned %d\n", result);
    return result;
}
