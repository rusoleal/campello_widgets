// Multi-window on the web (Phase 17 M4): one app, one widget tree, several
// canvases on one page.
//
//  - "#canvas"  the runApp() canvas;
//  - "#sidebar" a canvas laid out by the page (shell.html), opened as a
//               window with WindowOptions::host_element;
//  - Inspector  a floating canvas the framework creates (no host element),
//               with a TextField: each canvas has its own focus and keys;
//  - About      a dialog, centred over the main canvas;
//  - Preferences a modal dialog: the main canvas takes no input meanwhile;
//  - Options    a popup menu canvas that extends past the main canvas;
//  - Pop-out    (experimental) a separate browser window, window.open(),
//               drawn with the same GPU device; opened from a tap because
//               popup blockers refuse it otherwise.
//
// All of them show and change the same counter, held in the app's State:
// windows share state through the one element tree, with no messaging.
// Each change is also printed to the console ("[demo] ...").

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/web/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>

#include <cstdio>

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

class WebMultiWindowApp;

class WebMultiWindowState : public cw::State<WebMultiWindowApp>
{
public:
    void initState() override
    {
        sidebar_ = cw::WindowController::create({.title = "Sidebar", .host_element = "#sidebar"});
        openInspector();
    }

    void dispose() override
    {
        for (auto* w : {&sidebar_, &inspector_, &about_, &popout_, &prefs_})
            if (*w) (*w)->destroy();
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto increment = [this] {
            setState([this] { ++count_; });
            std::printf("[demo] count=%d\n", count_);
        };

        std::vector<cw::WidgetRef> actions{
            button("+1", kAccent, increment),
            button(inspector_ ? "Inspector is open" : "Open inspector", cw::Color::fromRGB(0.2f, 0.6f, 0.35f),
                   [this] { if (!inspector_) openInspector(); else inspector_->show(); }),
            button("About…", cw::Color::fromRGB(0.5f, 0.5f, 0.55f),
                   [this] { if (!about_) openAbout(); else about_->show(); }),
            button(popout_ ? "Pop-out is open" : "Pop out a window", cw::Color::fromRGB(0.55f, 0.3f, 0.7f),
                   [this] { if (!popout_) openPopout(); else popout_->show(); }),
            button("Preferences… (modal)", cw::Color::fromRGB(0.6f, 0.4f, 0.2f),
                   [this] { if (!prefs_) openPreferences(); }),
            optionsMenu(increment),
        };
        if (prefs_)
        {
            actions.push_back(std::make_shared<cw::Window>(prefs_, page(
                "Preferences", count_,
                {button("Done", cw::Color::fromRGB(0.2f, 0.6f, 0.35f), [this] { prefs_->destroy(); })},
                cw::Color::fromRGB(0.95f, 0.93f, 0.98f))));
        }

        // Every other window's content lives here, in the main tree; each
        // Window widget is an empty leaf in this canvas.
        if (sidebar_)
        {
            actions.push_back(std::make_shared<cw::Window>(sidebar_, page(
                "Sidebar", count_, {button("+1 from the sidebar", kAccent, increment)},
                cw::Color::fromRGB(0.92f, 0.94f, 0.98f))));
        }
        if (inspector_)
        {
            auto field = std::make_shared<cw::TextField>();
            actions.push_back(std::make_shared<cw::Window>(inspector_, page(
                "Inspector", count_,
                {button("+1 from the inspector", kAccent, increment),
                 cw::mw<cw::SizedBox>(std::optional<float>{220.0f}, std::optional<float>{40.0f}, field),
                 button("Close", cw::Color::fromRGB(0.85f, 0.2f, 0.15f), [this] { inspector_->requestClose(); })},
                cw::Color::fromRGB(0.93f, 0.97f, 0.94f))));
        }
        if (about_)
        {
            actions.push_back(std::make_shared<cw::Window>(about_, page(
                "About", count_,
                {button("Close", cw::Color::fromRGB(0.85f, 0.2f, 0.15f), [this] { about_->destroy(); })},
                cw::Color::fromRGB(0.97f, 0.95f, 0.92f))));
        }

        if (popout_)
        {
            actions.push_back(std::make_shared<cw::Window>(popout_, page(
                "Pop-out window", count_, {button("+1 from the pop-out", kAccent, increment)},
                cw::Color::fromRGB(0.96f, 0.93f, 0.98f))));
        }

        return page("Main canvas", count_, actions, cw::Color::fromRGB(0.97f, 0.97f, 0.98f));
    }

private:
    void openInspector()
    {
        inspector_ = cw::WindowController::create({.title = "Inspector", .size = {300.0f, 380.0f},
                                                   .position = cw::Offset{400.0f, 60.0f}});
        if (!inspector_) return;
        inspector_->on_destroyed = [this] {
            std::printf("[demo] inspector closed\n");
            setState([this] { inspector_.reset(); });
        };
        std::printf("[demo] inspector opened\n");
        setState([] {});
    }

    void openAbout()
    {
        about_ = cw::WindowController::create({.title = "About", .size = {280.0f, 240.0f},
                                               .kind = cw::WindowKind::dialog, .resizable = false});
        if (!about_) return;
        about_->on_destroyed = [this] {
            std::printf("[demo] about closed\n");
            setState([this] { about_.reset(); });
        };
        std::printf("[demo] about opened\n");
        setState([] {});
    }

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
            });

        auto anchor        = std::make_shared<cw::PopupWindowAnchor>();
        anchor->child      = button("Options ▾", cw::Color::fromRGB(0.3f, 0.3f, 0.35f),
                                    [this] { setState([this] { menu_open_ = !menu_open_; }); });
        anchor->popup      = menu_bg;
        anchor->open       = menu_open_;
        anchor->popup_size = {380.0f, 108.0f};
        anchor->offset     = {480.0f, 4.0f};  // reaches over the sidebar canvas
        anchor->on_dismiss = [this] { setState([this] { menu_open_ = false; }); };
        return anchor;
    }

    void openPreferences()
    {
        prefs_ = cw::WindowController::create({.title = "Preferences", .size = {300.0f, 240.0f},
                                               .kind = cw::WindowKind::dialog, .modal = true});
        if (!prefs_) return;
        prefs_->on_destroyed = [this] {
            std::printf("[demo] preferences closed\n");
            setState([this] { prefs_.reset(); });
        };
        std::printf("[demo] preferences opened\n");
        setState([] {});
    }

    void openPopout()
    {
        popout_ = cw::WindowController::create({.title = "Pop-out", .size = {320.0f, 300.0f},
                                                .web_popup = true});
        if (!popout_) { std::printf("[demo] pop-out blocked\n"); return; }
        popout_->on_destroyed = [this] {
            std::printf("[demo] pop-out closed\n");
            setState([this] { popout_.reset(); });
        };
        std::printf("[demo] pop-out opened\n");
        setState([] {});
    }

    int                                   count_ = 0;
    std::shared_ptr<cw::WindowController> sidebar_;
    std::shared_ptr<cw::WindowController> inspector_;
    std::shared_ptr<cw::WindowController> about_;
    std::shared_ptr<cw::WindowController> popout_;
    std::shared_ptr<cw::WindowController> prefs_;
    bool                                  menu_open_ = false;
};

class WebMultiWindowApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<WebMultiWindowState>();
    }
};

int main()
{
    cw::DebugFlags::showDebugBanner = false;
    return cw::runApp("campello_widgets — multi-window", std::make_shared<WebMultiWindowApp>());
}
