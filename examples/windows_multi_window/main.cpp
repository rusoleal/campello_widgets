// Multi-window example for Windows (Phase 17): one app, one widget tree,
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
// The self-tests post real Win32 input messages (WM_LBUTTONDOWN/UP, WM_CHAR,
// WM_KEYDOWN, WM_CLOSE) to the windows, so input goes through each window's
// actual window procedure, print what they observe, and, if
// CW_MULTIWINDOW_DEMO_DIR is set, save what each window shows on screen
// (PrintWindow, i.e. DWM's composited image) as <dir>/<step>_<title>.bmp.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/windows/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>
#include <campello_widgets/ui/view.hpp>

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cw = systems::leal::campello_widgets;

namespace
{
    cw::TextStyle style(float size, cw::Color color)
    {
        cw::TextStyle s{};
        s.font_family = "Segoe UI";
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
    // Self-test helpers: real Win32 messages to this process's windows.
    // ------------------------------------------------------------------

    std::string titleOf(HWND hwnd)
    {
        wchar_t buf[256] = {};
        GetWindowTextW(hwnd, buf, 256);
        char out[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out), nullptr, nullptr);
        return out;
    }

    bool isPopup(HWND hwnd)
    {
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        return wcscmp(cls, L"CampelloWidgetsPopup") == 0;
    }

    // This thread's top-level windows (ours), in z-order.
    std::vector<HWND> ourWindows(bool visible_only = true)
    {
        std::vector<HWND> out;
        EnumThreadWindows(GetCurrentThreadId(), [](HWND h, LPARAM lp) -> BOOL {
            wchar_t cls[64] = {};
            GetClassNameW(h, cls, 64);
            if (wcsncmp(cls, L"CampelloWidgets", 15) == 0 && wcscmp(cls, L"CampelloWidgetsHost") != 0)
                reinterpret_cast<std::vector<HWND>*>(lp)->push_back(h);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&out));
        if (visible_only)
            out.erase(std::remove_if(out.begin(), out.end(), [](HWND h) { return !IsWindowVisible(h); }),
                      out.end());
        return out;
    }

    // "" names the popup (it has no title).
    HWND windowTitled(const char* title)
    {
        for (HWND h : ourWindows())
        {
            if (*title == '\0' ? isPopup(h) : (!isPopup(h) && titleOf(h) == title)) return h;
        }
        return nullptr;
    }

    float dprOf(HWND hwnd) { return static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f; }

    // A click at (x, y): logical pixels from the top-left of the client area.
    void clickInWindow(const char* title, float x, float y)
    {
        HWND h = windowTitled(title);
        if (!h) { std::printf("[demo] no window titled '%s'\n", title); return; }
        const float  dpr = dprOf(h);
        const LPARAM pos = MAKELPARAM(static_cast<int>(x * dpr), static_cast<int>(y * dpr));
        PostMessageW(h, WM_MOUSEMOVE, 0, pos);
        PostMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, pos);
        PostMessageW(h, WM_LBUTTONUP, 0, pos);
        std::printf("[demo] clicked '%s' at (%.0f, %.0f)\n", *title ? title : "(popup)", x, y);
    }

    void typeInWindow(const char* title, const char* text)
    {
        HWND h = windowTitled(title);
        if (!h) { std::printf("[demo] no window titled '%s'\n", title); return; }
        for (const char* c = text; *c; ++c) PostMessageW(h, WM_CHAR, static_cast<WPARAM>(*c), 1);
        std::printf("[demo] typed '%s' into '%s'\n", text, title);
    }

    void pressEscape(const char* title)
    {
        HWND h = windowTitled(title);
        if (!h) { std::printf("[demo] no window '%s' for Escape\n", *title ? title : "(popup)"); return; }
        PostMessageW(h, WM_KEYDOWN, VK_ESCAPE, 1);
        PostMessageW(h, WM_KEYUP, VK_ESCAPE, 0xC0000001);
        std::printf("[demo] pressed Escape in '%s'\n", *title ? title : "(popup)");
    }

    // What the window shows on screen, as a 32-bit BMP.
    void saveWindowImage(HWND hwnd, const std::string& path)
    {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        const int w = rc.right, h = rc.bottom;
        if (w <= 0 || h <= 0) return;
        HDC screen = GetDC(nullptr);
        HDC mem    = CreateCompatibleDC(screen);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = -h;
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void*   bits = nullptr;
        HBITMAP bmp  = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old  = SelectObject(mem, bmp);
        if (PrintWindow(hwnd, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT) && bits)
        {
            if (FILE* f = std::fopen(path.c_str(), "wb"))
            {
                BITMAPFILEHEADER fh = {};
                fh.bfType    = 0x4D42;
                fh.bfOffBits = sizeof(fh) + sizeof(bi.bmiHeader);
                fh.bfSize    = fh.bfOffBits + static_cast<DWORD>(w * h * 4);
                std::fwrite(&fh, sizeof(fh), 1, f);
                std::fwrite(&bi.bmiHeader, sizeof(bi.bmiHeader), 1, f);
                std::fwrite(bits, 4, static_cast<size_t>(w) * h, f);
                std::fclose(f);
            }
        }
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }

    void captureAllWindows(const char* step)
    {
        const char* dir = std::getenv("CW_MULTIWINDOW_DEMO_DIR");
        if (!dir) return;
        for (HWND h : ourWindows())
        {
            std::string name = isPopup(h) ? "popup" : titleOf(h);
            for (char& c : name) if (c == ' ' || c == '-') c = '_';
            saveWindowImage(h, std::string(dir) + "/" + step + "_" + name + ".bmp");
        }
    }

    // Runs `steps` one after another, `interval_ms` apart, on the UI thread
    // (SetTimer callbacks are dispatched by the message loop).
    struct Script
    {
        std::vector<std::function<void()>> steps;
        size_t                              next = 0;
    };
    Script g_script;

    void runScript(std::vector<std::function<void()>> steps, UINT interval_ms)
    {
        g_script.steps = std::move(steps);
        g_script.next  = 0;
        SetTimer(nullptr, 0, interval_ms, [](HWND, UINT, UINT_PTR id, DWORD) {
            if (g_script.next >= g_script.steps.size()) { KillTimer(nullptr, id); return; }
            auto step = g_script.steps[g_script.next++];
            step();
            std::fflush(stdout);
        });
    }
}

class MultiWindowApp;

class MultiWindowState : public cw::State<MultiWindowApp>
{
public:
    void initState() override
    {
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
        HWND main  = windowTitled("Multi-window");
        HWND popup = windowTitled("");
        HWND front = GetForegroundWindow();
        std::printf("[demo] %s: count=%d inspector=%s about=%s prefs=%s menu=%s field='%s' "
                    "windows=%zu active='%s' main_enabled=%d main_blocked=%d\n",
                    step, count_, inspector_ ? "open" : "closed", about_ ? "open" : "closed",
                    prefs_ ? "open" : "closed", menu_open_ ? "open" : "closed", field_->text().c_str(),
                    ourWindows().size(),
                    front ? (isPopup(front) ? "(popup)" : titleOf(front).c_str()) : "(none)",
                    main ? (IsWindowEnabled(main) ? 1 : 0) : -1,
                    cw::View::primary() && cw::View::primary()->isInputBlocked() ? 1 : 0);
        if (popup && main)
        {
            RECT p = {}, m = {};
            GetWindowRect(popup, &p);
            GetWindowRect(main, &m);
            std::printf("[demo]   popup y=[%ld..%ld] main y=[%ld..%ld] -> extends below main: %s\n",
                        p.top, p.bottom, m.top, m.bottom, p.bottom > m.bottom ? "yes" : "no");
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
            [this] { typeInWindow("Inspector", "hi"); },
            [this] { report("typed into the inspector's field"); captureAllWindows("2_after_typing"); },
            [this] { typeInWindow("Multi-window", "x"); },  // the main window has no text field
            [this] { report("typed into the main window"); },
            [this] { clickInWindow("About", 150.0f, kAboutCloseY); },
            [this] { report("clicked Close in About"); captureAllWindows("3_after_about_close"); },
            [] { PostMessageW(windowTitled("Multi-window"), WM_CLOSE, 0, 0); },
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
            [] {
                // Activating another window takes activation from the popup.
                if (HWND main = windowTitled("Multi-window")) SetForegroundWindow(main);
                std::printf("[demo] activated the main window\n");
            },
            [this] { report("popup lost activation"); },
            [this] { openInspector(); openAbout(); },
            [this] {
                report("before closing the main window");
                std::printf("[demo] closing the main window (default policy: quit)\n");
                PostMessageW(windowTitled("Multi-window"), WM_CLOSE, 0, 0);
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
            [this] { report("inspector opened"); PostMessageW(windowTitled("Multi-window"), WM_CLOSE, 0, 0); },
            [this] { report("main window closed"); clickInWindow("Inspector", 180.0f, kInspectorPlusY); },
            [this] { report("clicked +1 in the inspector"); PostMessageW(windowTitled("Inspector"), WM_CLOSE, 0, 0); },
            [] { std::printf("[demo] STILL RUNNING after closing the last window\n"); },
        }, 600);
    }

    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {360.0f, 420.0f}});
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

    // Layout positions (logical pixels), read off the 0_layout captures.
    static constexpr float kMainPlusY       = 254.0f;
    static constexpr float kMainPrefsY      = 398.0f;
    static constexpr float kMainOptionsY    = 446.0f;
    static constexpr float kInspectorPlusY  = 270.0f;
    static constexpr float kInspectorFieldY = 313.0f;
    static constexpr float kAboutCloseY     = 200.0f;
    static constexpr float kPrefsPlusTenY   = 186.0f;
    static constexpr float kPrefsDoneY      = 234.0f;

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
    const int result = cw::runApp("Multi-window", 520, 560, std::make_shared<MultiWindowApp>());
    std::printf("[demo] runApp returned %d\n", result);
    return result;
}
