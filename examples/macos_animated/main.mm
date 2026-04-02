#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>

#include <string>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

static cw::TextStyle labelStyle(float size, cw::Color color)
{
    cw::TextStyle s{};
    s.font_family = "Helvetica Neue";
    s.font_size   = size;
    s.color       = color;
    return s;
}

static const cw::Color kBlue   = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
static const cw::Color kOrange = cw::Color::fromRGB(0.95f, 0.40f, 0.10f);
static const cw::Color kGreen  = cw::Color::fromRGB(0.10f, 0.70f, 0.40f);
static const cw::Color kPurple = cw::Color::fromRGB(0.60f, 0.20f, 0.80f);

// Wraps a widget in a MouseRegion that shows the pointer cursor on hover.
static cw::WidgetRef withPointerCursor(cw::WidgetRef child)
{
    auto region    = std::make_shared<cw::MouseRegion>();
    region->cursor = cw::SystemMouseCursor::pointer;
    region->child  = std::move(child);
    return region;
}

// ---------------------------------------------------------------------------
// Section 1 — Tween bar (explicit AnimationController + forward/reverse)
// ---------------------------------------------------------------------------

class TweenSection;

class TweenSectionState : public cw::State<TweenSection>
{
public:
    void initState() override
    {
        ctrl_ = std::make_shared<cw::AnimationController>(800.0);
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto bar_builder = std::make_shared<cw::AnimatedBuilder>();
        bar_builder->animation = ctrl_;
        bar_builder->builder = [ctrl = ctrl_](cw::BuildContext&) -> cw::WidgetRef
        {
            cw::CurvedAnimation curved(*ctrl, cw::Curves::easeInOut);
            const double t = curved.value();

            const float     w = cw::Tween<float>{24.0f, 280.0f}.evaluate(t);
            const cw::Color c = cw::Tween<cw::Color>{kBlue, kOrange}.evaluate(t);

            auto bar = std::make_shared<cw::Container>();
            bar->width  = w;
            bar->height = 24.0f;
            bar->color  = c;
            return bar;
        };

        auto col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Tween bar — tap to toggle",
                    labelStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f),
                    bar_builder),
            }
        );

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        box->child   = col;

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] {
            if (ctrl_->status() == cw::AnimationStatus::completed ||
                ctrl_->status() == cw::AnimationStatus::forward)
                ctrl_->reverse();
            else
                ctrl_->forward();
        };
        tap->child = box;
        return withPointerCursor(tap);
    }

private:
    std::shared_ptr<cw::AnimationController> ctrl_;
};

class TweenSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<TweenSectionState>();
    }
};

// ---------------------------------------------------------------------------
// Section 2 — AnimatedContainer (implicit animation)
// ---------------------------------------------------------------------------

class ImplicitSection;

class ImplicitSectionState : public cw::State<ImplicitSection>
{
public:
    void initState() override { expanded_ = false; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto card = std::make_shared<cw::AnimatedContainer>();
        card->duration_ms = 500.0;
        card->curve       = cw::Curves::easeInOut;
        card->color       = expanded_
            ? cw::Color::fromRGB(0.85f, 0.95f, 1.0f)
            : cw::Color::fromRGB(0.93f, 0.93f, 0.97f);
        card->width       = expanded_ ? 280.0f : 160.0f;
        card->height      = expanded_ ? 80.0f  : 40.0f;
        card->child       = cw::mw<cw::Center>(
            cw::mw<cw::Text>(
                expanded_ ? "shrink me" : "expand me",
                labelStyle(13.0f, cw::Color::fromRGB(0.2f, 0.2f, 0.4f))));

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] { setState([this] { expanded_ = !expanded_; }); };
        tap->child  = card;

        auto col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("AnimatedContainer — tap card",
                    labelStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f),
                    withPointerCursor(tap)),
            }
        );

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(0.97f, 1.0f, 0.97f);
        box->child   = col;
        return box;
    }

private:
    bool expanded_ = false;
};

class ImplicitSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ImplicitSectionState>();
    }
};

// ---------------------------------------------------------------------------
// Section 3 — AnimatedOpacity (fade in / fade out)
// ---------------------------------------------------------------------------

class OpacitySection;

class OpacitySectionState : public cw::State<OpacitySection>
{
public:
    void initState() override { visible_ = true; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto target = std::make_shared<cw::AnimatedOpacity>();
        target->duration_ms = 600.0;
        target->opacity     = visible_ ? 1.0f : 0.0f;
        cw::TextStyle helloStyle{};
        helloStyle.font_family = "Helvetica Neue";
        helloStyle.font_size   = 18.0f;
        helloStyle.color       = cw::Color::white();

        auto hello_box = std::make_shared<cw::Container>();
        hello_box->width  = 120.0f;
        hello_box->height = 48.0f;
        hello_box->color  = kGreen;
        hello_box->child  = cw::mw<cw::Center>(cw::mw<cw::Text>("hello!", helloStyle));
        target->child = hello_box;

        cw::TextStyle btnStyle{};
        btnStyle.font_family = "Helvetica Neue";
        btnStyle.font_size   = 13.0f;
        btnStyle.color       = cw::Color::white();

        auto toggle_btn = std::make_shared<cw::Container>();
        toggle_btn->padding = cw::EdgeInsets::symmetric(12.0f, 6.0f);
        toggle_btn->color   = kPurple;
        toggle_btn->child   = cw::mw<cw::Text>(visible_ ? "fade out" : "fade in", btnStyle);

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] { setState([this] { visible_ = !visible_; }); };
        tap->child  = toggle_btn;

        auto row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                withPointerCursor(tap),
                cw::mw<cw::SizedBox>(16.0f),
                target,
            }
        );

        auto col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("AnimatedOpacity — press button",
                    labelStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f), row),
            }
        );

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        box->child   = col;
        return box;
    }

private:
    bool visible_ = true;
};

class OpacitySection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<OpacitySectionState>();
    }
};

// ---------------------------------------------------------------------------
// Section 4 — Staggered animations (three bars with offset curves)
// ---------------------------------------------------------------------------

class StaggerSection;

class StaggerSectionState : public cw::State<StaggerSection>
{
public:
    void initState() override
    {
        ctrl_ = std::make_shared<cw::AnimationController>(1200.0);
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        using CurveFn = cw::CurvedAnimation::CurveFn;

        struct BarSpec { cw::Color color; CurveFn curve; };
        const BarSpec specs[] = {
            { kBlue,   cw::Curves::easeIn    },
            { kGreen,  cw::Curves::easeInOut },
            { kOrange, cw::Curves::easeOut   },
        };

        std::vector<cw::WidgetRef> bars;
        for (int i = 0; i < 3; ++i)
        {
            cw::Color  bar_color = specs[i].color;
            CurveFn    bar_curve = specs[i].curve;

            auto bar_builder = std::make_shared<cw::AnimatedBuilder>();
            bar_builder->animation = ctrl_;
            bar_builder->builder = [ctrl = ctrl_, bar_color, bar_curve](cw::BuildContext&) -> cw::WidgetRef
            {
                cw::CurvedAnimation curved(*ctrl, bar_curve);
                const double t = curved.value();
                const float  w = cw::Tween<float>{16.0f, 260.0f}.evaluate(t);

                auto bar = std::make_shared<cw::Container>();
                bar->width  = w;
                bar->height = 20.0f;
                bar->color  = bar_color;
                return bar;
            };

            bars.push_back(cw::mw<cw::Padding>(
                cw::EdgeInsets::only(0.0f, i > 0 ? 6.0f : 0.0f, 0.0f, 0.0f),
                bar_builder));
        }

        auto bars_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            bars
        );

        auto bars_box = std::make_shared<cw::Container>();
        bars_box->padding = cw::EdgeInsets::all(12.0f);
        bars_box->color   = cw::Color::fromRGB(0.96f, 0.96f, 0.99f);
        bars_box->child   = bars_col;

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] {
            if (ctrl_->status() == cw::AnimationStatus::completed ||
                ctrl_->status() == cw::AnimationStatus::forward)
                ctrl_->reverse();
            else
                ctrl_->forward();
        };
        tap->child = bars_box;

        auto col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Different curves — tap to toggle",
                    labelStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f),
                    withPointerCursor(tap)),
            }
        );

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(1.0f, 0.97f, 0.97f);
        box->child   = col;
        return box;
    }

private:
    std::shared_ptr<cw::AnimationController> ctrl_;
};

class StaggerSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<StaggerSectionState>();
    }
};

// ---------------------------------------------------------------------------
// Root app
// ---------------------------------------------------------------------------

class AnimatedApp : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 18.0f;
        titleStyle.color       = cw::Color::fromRGB(0.15f, 0.15f, 0.20f);

        auto title_bar = std::make_shared<cw::Container>();
        title_bar->padding = cw::EdgeInsets::symmetric(16.0f, 14.0f);
        title_bar->color   = cw::Color::fromRGB(0.93f, 0.93f, 0.97f);
        title_bar->child   = cw::mw<cw::Text>("Animation Gallery", titleStyle);

        auto divider = std::make_shared<cw::Container>();
        divider->height = 1.0f;
        divider->color  = cw::Color::fromRGB(0.82f, 0.82f, 0.88f);

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<TweenSection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<ImplicitSection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<OpacitySection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<StaggerSection>(),
            }
        );

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                title_bar,
                divider,
                cw::mw<cw::Expanded>(scroll),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        bg->child = root;
        return bg;
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    return cw::runApp(
        std::make_shared<AnimatedApp>(),
        "campello_widgets — Animated Transitions",
        420.0f,
        620.0f);
}
