// Multi-window on iPadOS (Phase 17 M5): one app, one widget tree, several
// UIWindowScenes.
//
// The main window opens an "Inspector" window (a new scene, shown next to
// it by iPadOS). Windows the user opens from the dock / app switcher are
// adopted through WindowController::setPlatformWindowHandler() and show
// the same counter, as does an external display if one is connected. All
// of them share the counter held in the app's State.
//
// CW_SCENES_DEMO=2 (pass as SIMCTL_CHILD_CW_SCENES_DEMO=2 to simctl launch)
// runs a self-test: it opens the inspector, taps its "+1" (pointer events
// into that window's View), asks iPadOS for a window the way the dock does,
// closes the inspector, and prints "[demo] ..." lines.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/ios/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/view.hpp>

#import <UIKit/UIKit.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>

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

    cw::WidgetRef button(const std::string& label, cw::Color color, std::function<void()> on_press,
                         float height = 48.0f)
    {
        auto box    = std::make_shared<cw::Container>();
        box->color  = color;
        box->height = height;
        box->child  = cw::mw<cw::Center>(cw::mw<cw::Text>(label, style(17.0f, cw::Color::white())));
        auto tap    = std::make_shared<cw::GestureDetector>();
        tap->on_tap = std::move(on_press);
        tap->child  = box;
        return cw::mw<cw::Padding>(cw::EdgeInsets::all(8.0f), tap);
    }

    // A window's page: the title and shared counter in the top half, the
    // actions in the bottom half (the self-test taps at 75% of the height).
    cw::WidgetRef page(const std::string& title, int count, std::vector<cw::WidgetRef> actions,
                       cw::Color background)
    {
        auto header = cw::mw<cw::Center>(cw::mw<cw::Column>(
            cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
            std::vector<cw::WidgetRef>{
                cw::mw<cw::Text>(title, style(24.0f, kInk)),
                cw::mw<cw::Text>("shared counter", style(15.0f, kMuted)),
                cw::mw<cw::Text>(std::to_string(count), style(72.0f, kAccent)),
            }));
        auto body = cw::mw<cw::Column>(cw::MainAxisAlignment::center, cw::CrossAxisAlignment::stretch, actions);

        auto bg   = std::make_shared<cw::Container>();
        bg->color = background;
        bg->child = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            std::vector<cw::WidgetRef>{ cw::mw<cw::Expanded>(header), cw::mw<cw::Expanded>(body) });
        return bg;
    }

    void after(double seconds, void (^block)())
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), block);
    }

    int connectedScenes() { return (int)UIApplication.sharedApplication.connectedScenes.count; }

    // stdout (simctl launch --console-pty) and the system log (log show).
    void say(const char* format, ...) __attribute__((format(printf, 1, 2)));
    void say(const char* format, ...)
    {
        char line[512];
        va_list args;
        va_start(args, format);
        std::vsnprintf(line, sizeof line, format, args);
        va_end(args);
        std::printf("[demo] %s\n", line);
        std::fflush(stdout);
        NSLog(@"[demo] %s", line);
    }
}

class ScenesApp;

class ScenesState : public cw::State<ScenesApp>
{
public:
    void initState() override
    {
        // Windows iPadOS opens on its own (the user asked for a new window,
        // or an external display connected): show the counter there too.
        cw::WindowController::setPlatformWindowHandler([this](std::shared_ptr<cw::WindowController> w) {
            const bool display = w->kind() == cw::WindowKind::external_display;
            say("platform opened a %s", display ? "external display" : "window");
            auto* raw = w.get();
            w->on_destroyed = [this, raw] {
                say("extra window closed");
                setState([this, raw] {
                    std::erase_if(extra_, [raw](auto& e) { return e.get() == raw; });
                });
            };
            setState([this, w] { extra_.push_back(w); });
        });

        const char* demo = std::getenv("CW_SCENES_DEMO");
        say("initState (CW_SCENES_DEMO=%s, scenes=%d)", demo ? demo : "unset", connectedScenes());
        if (demo && *demo == '2') runSelfTest();
    }

    void dispose() override
    {
        cw::WindowController::setPlatformWindowHandler(nullptr);
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto increment = [this] { setState([this] { ++count_; }); };

        std::vector<cw::WidgetRef> actions{
            button("+1", kAccent, increment),
            button(inspector_ ? "Inspector is open" : "Open inspector window",
                   cw::Color::fromRGB(0.2f, 0.6f, 0.35f),
                   [this] { if (!inspector_) openInspector(); else inspector_->show(); }),
        };

        if (inspector_)
        {
            auto field = std::make_shared<cw::TextField>();
            actions.push_back(std::make_shared<cw::Window>(inspector_, page(
                "Inspector", count_,
                {button("+1 from the inspector", kAccent, increment, 120.0f),
                 cw::mw<cw::Padding>(cw::EdgeInsets::all(8.0f), cw::mw<cw::SizedBox>(
                     std::optional<float>{}, std::optional<float>{44.0f}, field)),
                 button("Close", cw::Color::fromRGB(0.85f, 0.2f, 0.15f),
                        [this] { inspector_->requestClose(); })},
                cw::Color::fromRGB(0.93f, 0.97f, 0.94f))));
        }
        for (auto& w : extra_)
        {
            const bool display = w->kind() == cw::WindowKind::external_display;
            actions.push_back(std::make_shared<cw::Window>(w, page(
                display ? "External display" : "Another window", count_,
                display ? std::vector<cw::WidgetRef>{}
                        : std::vector<cw::WidgetRef>{button("+1 from here", kAccent, increment)},
                cw::Color::fromRGB(0.96f, 0.93f, 0.98f))));
        }

        return page("Main window", count_, actions, cw::Color::fromRGB(0.97f, 0.97f, 0.98f));
    }

private:
    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {420.0f, 600.0f}});
        if (!inspector_)
        {
            say("no inspector: this device has a single window (iPhone)");
            return;
        }
        inspector_->on_destroyed = [this] {
            say("inspector closed");
            setState([this] { inspector_.reset(); });
        };
        setState([] {});
    }

    // A tap at (x, y) in a window's own coordinates, through its View.
    static void tap(cw::View* view, float x, float y)
    {
        if (!view) return;
        cw::View::Scope scope(view);
        for (auto kind : {cw::PointerEventKind::down, cw::PointerEventKind::up})
        {
            cw::PointerEvent e;
            e.kind        = kind;
            e.pointer_id  = 77;
            e.position    = {x, y};
            e.pressure    = kind == cw::PointerEventKind::down ? 1.0f : 0.0f;
            e.device_kind = cw::PointerDeviceKind::touch;
            view->pointerDispatcher().handlePointerEvent(e);
        }
    }

    void report(const char* step)
    {
        say("%s: count=%d inspector=%s extra=%zu scenes=%d", step, count_,
                    inspector_ ? (inspector_->metrics().logical_size.width > 0 ? "open" : "pending")
                               : "closed",
                    extra_.size(), connectedScenes());
    }

    void runSelfTest()
    {
        after(1.5, ^{
            report("launch");
            openInspector();
        });
        after(4.0, ^{
            report("inspector requested");
            if (!inspector_) return;
            const cw::WindowMetrics m = inspector_->metrics();
            say("inspector metrics %.0fx%.0f @%.0fx", m.logical_size.width,
                        m.logical_size.height, m.device_pixel_ratio);
            // "+1 from the inspector" is the first of the bottom half's
            // centred actions (136 + 60 + 64 = 260 points tall in all).
            const float h = m.logical_size.height - m.padding.vertical();
            const float y = h * 0.5f + (h * 0.5f - 260.0f) * 0.5f + 68.0f;
            tap(inspector_->view(), m.logical_size.width / 2.0f, y);
            tap(inspector_->view(), m.logical_size.width / 2.0f, y);
        });
        after(8.0, ^{
            report("after 2 inspector taps");
            say("SCREENSHOT inspector");
        });
        after(9.0, ^{ if (inspector_) inspector_->requestClose(); });
        after(11.0, ^{
            report("inspector closed");
            // What the dock's "+" does: a new scene without our activity.
            if (@available(iOS 17.0, *)) {
                [UIApplication.sharedApplication
                    activateSceneSessionForRequest:[UISceneSessionActivationRequest request]
                                      errorHandler:^(NSError* e) { NSLog(@"[demo] %@", e); }];
            }
        });
        after(14.0, ^{
            report("user window requested");
            say("SCREENSHOT user window");
            if (!extra_.empty()) extra_.front()->requestClose();
        });
        after(17.0, ^{
            report("user window closed");
            say("done");
        });
    }

    int                                                 count_ = 0;
    std::shared_ptr<cw::WindowController>               inspector_;
    std::vector<std::shared_ptr<cw::WindowController>>  extra_;
};

class ScenesApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ScenesState>();
    }
};

int main(int argc, char* argv[])
{
    return cw::runApp(argc, argv, std::make_shared<ScenesApp>());
}
