// Multi-window example (Phase 17): one app, one widget tree, three windows.
//
// The main window opens an "Inspector" (a regular window) and an "About"
// dialog. All three show and change the same counter, which lives in the
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

        return page("Main window", count_, actions, cw::Color::fromRGB(0.97f, 0.97f, 0.98f));
    }

private:
    // Clicks at (x, y) — logical pixels from the top-left of the window's
    // content — through AppKit, like a real mouse click.
    static void clickInWindow(const char* title, float x, float y)
    {
        for (NSWindow* w in NSApp.windows)
        {
            if (![w.title isEqualToString:[NSString stringWithUTF8String:title]]) continue;
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

    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {360.0f, 420.0f}});
        if (!inspector_) return;
        // Closed by the user: drop it so its Window widget leaves the tree.
        inspector_->on_destroyed = [this] { setState([this] { inspector_.reset(); }); };
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
