// Multi-window demo for Android (Phase 17 M6), enabled in the gallery app
// with `adb shell setprop debug.cw_multiwindow 1`.
//
// The main window opens an "Inspector" window: another activity in its own
// task (next to the app in split screen, a freeform window in desktop
// windowing). Both show and change one counter held in the app's State:
// windows share state through the one widget tree. Events are logged as
// "[demo] ..." (logcat tag campello_widgets.stdio).

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>
#include <campello_widgets/widgets/overlay.hpp>

#include <cstdio>
#include <vector>

namespace cw = systems::leal::campello_widgets;

namespace
{
    cw::TextStyle demoStyle(float size, cw::Color color)
    {
        cw::TextStyle s{};
        s.font_size = size;
        s.color     = color;
        return s;
    }

    const cw::Color kDemoInk    = cw::Color::fromRGB(0.15f, 0.15f, 0.18f);
    const cw::Color kDemoMuted  = cw::Color::fromRGB(0.45f, 0.45f, 0.50f);
    const cw::Color kDemoAccent = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);

    cw::WidgetRef demoButton(const std::string& label, cw::Color color, std::function<void()> on_press)
    {
        auto box    = std::make_shared<cw::Container>();
        box->color  = color;
        box->height = 64.0f;
        box->child  = cw::mw<cw::Center>(cw::mw<cw::Text>(label, demoStyle(18.0f, cw::Color::white())));
        auto tap    = std::make_shared<cw::GestureDetector>();
        tap->on_tap = std::move(on_press);
        tap->child  = box;
        return cw::mw<cw::Padding>(cw::EdgeInsets::all(10.0f), tap);
    }

    // Title + shared counter in the top half, actions in the bottom half.
    cw::WidgetRef demoPage(const std::string& title, int count, std::vector<cw::WidgetRef> actions,
                           cw::Color background)
    {
        auto header = cw::mw<cw::Center>(cw::mw<cw::Column>(
            cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
            std::vector<cw::WidgetRef>{
                cw::mw<cw::Text>(title, demoStyle(26.0f, kDemoInk)),
                cw::mw<cw::Text>("shared counter", demoStyle(16.0f, kDemoMuted)),
                cw::mw<cw::Text>(std::to_string(count), demoStyle(80.0f, kDemoAccent)),
            }));
        auto body = cw::mw<cw::Column>(cw::MainAxisAlignment::center, cw::CrossAxisAlignment::stretch, actions);
        auto bg   = std::make_shared<cw::Container>();
        bg->color = background;
        bg->child = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            std::vector<cw::WidgetRef>{ cw::mw<cw::Expanded>(header), cw::mw<cw::Expanded>(body) });
        return bg;
    }
}

class MultiWindowDemo;

class MultiWindowDemoState : public cw::State<MultiWindowDemo>
{
public:
    void initState() override
    {
        // Windows Android starts by itself (another launcher instance).
        cw::WindowController::setPlatformWindowHandler([this](std::shared_ptr<cw::WindowController> w) {
            std::printf("[demo] platform opened a window\n");
            auto* raw = w.get();
            w->on_destroyed = [this, raw] {
                setState([this, raw] { std::erase_if(extra_, [raw](auto& e) { return e.get() == raw; }); });
            };
            setState([this, w] { extra_.push_back(w); });
        });
    }

    void dispose() override { cw::WindowController::setPlatformWindowHandler(nullptr); }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto increment = [this] {
            setState([this] { ++count_; });
            std::printf("[demo] count=%d\n", count_);
        };

        std::vector<cw::WidgetRef> actions{
            demoButton("+1", kDemoAccent, increment),
            demoButton(inspector_ ? "Inspector is open" : "Open inspector window",
                       cw::Color::fromRGB(0.2f, 0.6f, 0.35f),
                       [this] { if (!inspector_) openInspector(); }),
            optionsMenu(increment),
        };

        if (inspector_)
        {
            auto field = std::make_shared<cw::TextField>();
            actions.push_back(std::make_shared<cw::Window>(inspector_, demoPage(
                "Inspector", count_,
                {demoButton("+1 from the inspector", kDemoAccent, increment),
                 cw::mw<cw::Padding>(cw::EdgeInsets::all(10.0f),
                     cw::mw<cw::SizedBox>(std::optional<float>{}, std::optional<float>{56.0f}, field)),
                 demoButton("Close", cw::Color::fromRGB(0.85f, 0.2f, 0.15f),
                            [this] { inspector_->requestClose(); })},
                cw::Color::fromRGB(0.93f, 0.97f, 0.94f))));
        }
        for (auto& w : extra_)
        {
            actions.push_back(std::make_shared<cw::Window>(w, demoPage(
                "Another window", count_, {demoButton("+1 from here", kDemoAccent, increment)},
                cw::Color::fromRGB(0.96f, 0.93f, 0.98f))));
        }

        return demoPage("Main window", count_, actions, cw::Color::fromRGB(0.97f, 0.97f, 0.98f));
    }

private:
    // A popup menu: no popup windows on Android, so it's drawn in this
    // window's Overlay (PopupWindowAnchor's fallback).
    cw::WidgetRef optionsMenu(std::function<void()> increment)
    {
        auto item = [this](const std::string& label, std::function<void()> action) {
            return demoButton(label, cw::Color::fromRGB(0.25f, 0.25f, 0.3f), [this, action] {
                if (action) action();
                setState([this] { menu_open_ = false; });
            });
        };
        auto menu_bg   = std::make_shared<cw::Container>();
        menu_bg->color = cw::Color::fromRGB(0.16f, 0.16f, 0.19f);
        menu_bg->child = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            std::vector<cw::WidgetRef>{
                item("+1 from the menu", increment),
                item("Reset counter", [this] { setState([this] { count_ = 0; }); std::printf("[demo] reset\n"); }),
            });
        auto anchor        = std::make_shared<cw::PopupWindowAnchor>();
        anchor->child      = demoButton("Options", cw::Color::fromRGB(0.3f, 0.3f, 0.35f),
                                        [this] { setState([this] { menu_open_ = !menu_open_; }); });
        anchor->popup      = menu_bg;
        anchor->open       = menu_open_;
        anchor->popup_size = {300.0f, 168.0f};
        anchor->offset     = {40.0f, -250.0f};  // above the button: it's near the bottom
        anchor->on_dismiss = [this] { setState([this] { menu_open_ = false; }); };
        return anchor;
    }

    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {360.0f, 640.0f}});
        if (!inspector_)
        {
            std::printf("[demo] could not open the inspector\n");
            return;
        }
        std::printf("[demo] inspector opened\n");
        inspector_->on_destroyed = [this] {
            std::printf("[demo] inspector closed (count=%d)\n", count_);
            setState([this] { inspector_.reset(); });
        };
        setState([] {});
    }

    int                                                 count_ = 0;
    bool                                                menu_open_ = false;
    std::shared_ptr<cw::WindowController>               inspector_;
    std::vector<std::shared_ptr<cw::WindowController>>  extra_;
};

class MultiWindowDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<MultiWindowDemoState>();
    }
};

cw::WidgetRef buildMultiWindowDemo()
{
    // An Overlay for the main window (popup menus fall back to it here).
    return std::make_shared<cw::Overlay>(std::vector<std::shared_ptr<cw::OverlayEntry>>{
        std::make_shared<cw::OverlayEntry>(std::make_shared<MultiWindowDemo>())});
}
