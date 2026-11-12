// Multi-window example (Phase 17): one app, one widget tree, three windows.
//
// The main window opens an "Inspector" (a regular window), an "About"
// dialog, a modal "Preferences" window (the main window takes no input while
// it is open) and an "Options" popup menu, a borderless window that extends
// past the main window's edge. All three show and change the same counter, which lives in the
// app's State: windows share state through the one element tree, with no
// messaging. The inspector also has a TextField, to show that each window
// has its own focus and text input.
//
// CW_MULTIWINDOW_DEMO=1 opens both secondary windows at launch (handy for
// screenshots). CW_MULTIWINDOW_DEMO=2 additionally runs a self-test: it sends
// synthesized clicks through AppKit (the inspector's "+1", then the About
// dialog's "Close"), writes each window's next frame to
// $CW_MULTIWINDOW_DEMO_DIR/<window>.png, prints what it did, and quits.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>

#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/view.hpp>

#import <Cocoa/Cocoa.h>

#include <cstdio>
#include <cstdlib>

namespace cw = systems::leal::campello_widgets;

namespace
{
    cw::TextStyle style(float size, cw::Color color)
    {
        cw::TextStyle s{};
        s.font_family = "Helvetica Neue";
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
}

class MultiWindowApp;

class MultiWindowState : public cw::State<MultiWindowApp>
{
public:
    void initState() override
    {
        if (const char* demo = std::getenv("CW_MULTIWINDOW_DEMO"); demo && *demo == '1')
        {
            count_ = 3;
            openInspector();
            openAbout();
        }
        if (const char* demo = std::getenv("CW_MULTIWINDOW_DEMO"); demo && *demo == '3')
        {
            count_ = 3;
            runModalAndPopupTest();
        }
        if (const char* demo = std::getenv("CW_MULTIWINDOW_DEMO"); demo && *demo == '2')
        {
            count_ = 3;
            openInspector();
            openAbout();
            runSelfTest();
        }
    }

    void dispose() override
    {
        // Close our windows with us.
        if (inspector_) inspector_->destroy();
        if (about_) about_->destroy();
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto increment = [this] { setState([this] { ++count_; }); };

        std::vector<cw::WidgetRef> actions{
            button("+1", kAccent, increment),
            button(inspector_ ? "Inspector is open" : "Open inspector", cw::Color::fromRGB(0.2f, 0.6f, 0.35f),
                   [this] { if (!inspector_) openInspector(); else inspector_->show(); }),
            button("About…", cw::Color::fromRGB(0.5f, 0.5f, 0.55f),
                   [this] { if (!about_) openAbout(); else about_->show(); }),
            button("Preferences… (modal)", cw::Color::fromRGB(0.55f, 0.3f, 0.7f),
                   [this] { if (!prefs_) openPreferences(); }),
            optionsMenu(increment),
        };

        // The secondary windows' content lives right here in the main tree;
        // each Window widget is an empty leaf in this window.
        if (inspector_)
        {
            auto field = std::make_shared<cw::TextField>();
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

    // "Options ▾" and its popup menu (a native popup window).
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
        anchor->child      = button("Options ▾", cw::Color::fromRGB(0.3f, 0.3f, 0.35f),
                                    [this] { setState([this] { menu_open_ = !menu_open_; }); });
        anchor->popup      = menu_bg;
        anchor->open       = menu_open_;
        anchor->popup_size = {380.0f, 156.0f};
        anchor->on_dismiss = [this] { setState([this] { menu_open_ = false; }); };
        return anchor;
    }

private:
    // Clicks at (x, y) — logical pixels from the top-left of the window's
    // content — through AppKit, like a real mouse click.
    static void clickInWindow(const char* title, float x, float y)
    {
        if (NSWindow* w = windowTitled(title))
        {
            const NSPoint p = NSMakePoint(x, w.contentView.bounds.size.height - y);
            for (NSEventType type : {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp})
            {
                NSEvent* e = [NSEvent mouseEventWithType:type location:p modifierFlags:0
                                               timestamp:NSProcessInfo.processInfo.systemUptime
                                            windowNumber:w.windowNumber context:nil
                                             eventNumber:0 clickCount:1 pressure:1.0f];
                [w sendEvent:e];
            }
            std::printf("[demo] clicked '%s' at (%.0f, %.0f)\n", title, x, y);
            return;
        }
        std::printf("[demo] no window titled '%s'\n", title);
    }

    // Types `text` into the window as real key events (keyDown/keyUp).
    static void typeInWindow(const char* title, const char* text)
    {
        for (NSWindow* w in NSApp.windows)
        {
            if (![w.title isEqualToString:[NSString stringWithUTF8String:title]]) continue;
            [w makeKeyAndOrderFront:nil];
            for (const char* c = text; *c; ++c)
            {
                NSString* chars = [NSString stringWithFormat:@"%c", *c];
                for (NSEventType type : {NSEventTypeKeyDown, NSEventTypeKeyUp})
                {
                    NSEvent* e = [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:0
                                                 timestamp:NSProcessInfo.processInfo.systemUptime
                                              windowNumber:w.windowNumber context:nil
                                                characters:chars charactersIgnoringModifiers:chars
                                                 isARepeat:NO keyCode:0];
                    [w sendEvent:e];
                }
            }
            std::printf("[demo] typed '%s' into '%s'\n", text, title);
            return;
        }
    }

    void captureNextFrames(const char* step)
    {
        const char* dir = std::getenv("CW_MULTIWINDOW_DEMO_DIR");
        if (!dir) return;
        auto capture = [&](cw::View* view, const char* name) {
            if (!view || !view->renderer()) return;
            view->renderer()->requestFrameCapture(std::string(dir) + "/" + step + "_" + name + ".png");
        };
        capture(cw::View::primary(), "main");
        if (inspector_) capture(inspector_->view(), "inspector");
        if (about_)     capture(about_->view(), "about");
    }

    void runSelfTest()
    {
        auto after = [](double seconds, void (^block)()) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), block);
        };
        after(1.5, ^{
            captureNextFrames("1_after_inspector_click");
            clickInWindow("Inspector", 180.0f, 270.0f);  // "+1 from the inspector"
        });
        after(2.5, ^{
            // Focus the inspector's TextField, then type into that window.
            clickInWindow("Inspector", 180.0f, 313.0f);
        });
        after(3.0, ^{
            captureNextFrames("2_after_typing");
            typeInWindow("Inspector", "hi");
        });
        after(4.0, ^{
            std::printf("[demo] count=%d inspector=%s about=%s\n", count_,
                        inspector_ ? "open" : "closed", about_ ? "open" : "closed");
            captureNextFrames("3_after_about_close");
            clickInWindow("About", 150.0f, 200.0f);      // "Close"
        });
        after(5.5, ^{
            int visible = 0;
            for (NSWindow* w in NSApp.windows) if (w.isVisible) ++visible;
            std::printf("[demo] count=%d inspector=%s about=%s visible_windows=%d\n", count_,
                        inspector_ ? "open" : "closed", about_ ? "open" : "closed", visible);
            std::fflush(stdout);
            [NSApp terminate:nil];
        });
    }

    // The visible window titled `title` (a popup has an empty title; AppKit
    // may also keep hidden windows of its own with an empty title).
    static NSWindow* windowTitled(const char* title)
    {
        NSWindow* hidden = nil;
        for (NSWindow* w in NSApp.windows)
        {
            if (![w.title isEqualToString:[NSString stringWithUTF8String:title]]) continue;
            if (w.isVisible) return w;
            if (!hidden) hidden = w;
        }
        return hidden;
    }

    static int visibleWindows()
    {
        int n = 0;
        for (NSWindow* w in NSApp.windows) if (w.isVisible) ++n;
        return n;
    }

    // Presses Escape in the window titled `title` (the app may not be
    // active when run from a terminal, so there may be no key window).
    static void pressEscape(const char* title)
    {
        NSWindow* w = windowTitled(title);
        if (!w) { std::printf("[demo] no window '%s' for Escape\n", title); return; }
        for (NSEventType type : {NSEventTypeKeyDown, NSEventTypeKeyUp})
        {
            NSEvent* e = [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:0
                                         timestamp:NSProcessInfo.processInfo.systemUptime
                                      windowNumber:w.windowNumber context:nil
                                        characters:@"\x1b" charactersIgnoringModifiers:@"\x1b"
                                         isARepeat:NO keyCode:53];
            [w sendEvent:e];
        }
        std::printf("[demo] pressed Escape in '%s'\n", w.title.UTF8String);
    }

    // Captures every window's next frame as <dir>/<step>_<n>.png.
    void captureAllWindows(const char* step)
    {
        const char* dir = std::getenv("CW_MULTIWINDOW_DEMO_DIR");
        if (!dir) return;
        int n = 0;
        for (cw::View* view : cw::View::all())
        {
            if (!view->renderer()) continue;
            view->renderer()->requestFrameCapture(std::string(dir) + "/" + step + "_" + std::to_string(n++) + ".png");
            view->renderer()->notePaintRequested();  // a full frame even if nothing changed
            view->requestFrame();
        }
    }

    void report(const char* step)
    {
        NSWindow* popup = windowTitled("");
        NSWindow* main  = windowTitled("Multi-window");
        NSWindow* front = NSApp.orderedWindows.firstObject;
        std::printf("[demo] %s: count=%d prefs=%s menu=%s front='%s' visible_windows=%d main_blocked=%d\n",
                    step, count_, prefs_ ? "open" : "closed", menu_open_ ? "open" : "closed",
                    front ? front.title.UTF8String : "(none)", visibleWindows(),
                    cw::View::primary() && cw::View::primary()->isInputBlocked() ? 1 : 0);
        if (popup && popup.isVisible && main)
        {
            const NSRect p = popup.frame, m = main.frame;
            std::printf("[demo]   popup frame y=[%.0f..%.0f] main frame y=[%.0f..%.0f] -> extends below main: %s\n",
                        NSMinY(p), NSMaxY(p), NSMinY(m), NSMaxY(m), NSMinY(p) < NSMinY(m) ? "yes" : "no");
        }
        std::fflush(stdout);
    }

    // CW_MULTIWINDOW_DEMO=3: modal windows and popup menus (Phase 17 M2/M7),
    // through synthesized AppKit events. Coordinates are logical pixels from
    // the top-left of each window's content (see 0_layout captures).
    void runModalAndPopupTest()
    {
        auto after = [](double seconds, void (^block)()) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), block);
        };
        after(1.0, ^{
            report("start");
            clickInWindow("Multi-window", 260.0f, 398.0f);  // "Preferences… (modal)"
        });
        after(2.0, ^{
            report("preferences opened");
            captureAllWindows("1_modal");
            clickInWindow("Multi-window", 260.0f, 254.0f);  // "+1", blocked
        });
        after(2.4, ^{
            // Another window to the front first, to see the blocked click
            // bring the modal back.
            [windowTitled("Multi-window") orderFront:nil];
            report("main window ordered to the front");
            clickInWindow("Multi-window", 260.0f, 254.0f);  // "+1", blocked
        });
        after(2.8, ^{
            report("clicked +1 in the blocked main window");
            clickInWindow("Preferences", 160.0f, 234.0f);    // "Done"
        });
        after(3.6, ^{
            report("preferences closed");
            clickInWindow("Multi-window", 260.0f, 254.0f);  // "+1", works again
        });
        after(4.2, ^{
            report("clicked +1 again");
            clickInWindow("Multi-window", 260.0f, 446.0f);  // "Options ▾"
        });
        after(5.0, ^{
            report("menu opened");
            captureAllWindows("2_menu");
        });
        after(5.6, ^{
            clickInWindow("", 190.0f, 24.0f);                // "+1 from the menu"
        });
        after(6.0, ^{
            report("menu item chosen");
            clickInWindow("Multi-window", 260.0f, 446.0f);  // open again
        });
        after(6.8, ^{
            report("menu reopened");
            clickInWindow("Multi-window", 260.0f, 120.0f);  // outside it
        });
        after(7.6, ^{
            report("clicked outside the menu");
            clickInWindow("Multi-window", 260.0f, 446.0f);  // open again
        });
        after(8.4, ^{
            report("menu reopened");
            pressEscape("");  // the popup
        });
        after(9.2, ^{
            report("pressed Escape");
            std::printf("[demo] closing the main window (default policy: quit)\n");
            std::fflush(stdout);
            [windowTitled("Multi-window") performClose:nil];
        });
        after(12.0, ^{
            std::printf("[demo] still running after closing the main window\n");
            std::fflush(stdout);
            [NSApp terminate:nil];
        });
    }

    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {360.0f, 420.0f}});
        if (!inspector_) return;
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

    int                                   count_ = 0;
    std::shared_ptr<cw::WindowController> inspector_;
    std::shared_ptr<cw::WindowController> about_;
    std::shared_ptr<cw::WindowController> prefs_;
    bool                                  menu_open_ = false;
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
    return cw::runApp(std::make_shared<MultiWindowApp>(), "Multi-window", 520.0f, 560.0f);
}
