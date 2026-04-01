#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>

#include <string>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

// Wraps a widget in a MouseRegion that shows the pointer cursor on hover.
static cw::WidgetRef withPointerCursor(cw::WidgetRef child)
{
    auto region    = std::make_shared<cw::MouseRegion>();
    region->cursor = cw::SystemMouseCursor::pointer;
    region->child  = std::move(child);
    return region;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static cw::TextStyle makeStyle(float size, cw::Color color)
{
    cw::TextStyle s{};
    s.font_family = "Helvetica Neue";
    s.font_size   = size;
    s.color       = color;
    return s;
}

static std::string fmtFloat(float v)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << v;
    return ss.str();
}

// ---------------------------------------------------------------------------
// AnimationSection — exercises AnimationController + AnimatedBuilder
// ---------------------------------------------------------------------------

class AnimationSection;

class AnimationSectionState : public cw::State<AnimationSection>
{
public:
    void initState() override
    {
        ctrl_ = std::make_shared<cw::AnimationController>(600.0); // 600 ms
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto labelStyle = makeStyle(13.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f));
        auto hintStyle  = makeStyle(12.0f, cw::Color::fromRGB(0.65f, 0.65f, 0.65f));

        // AnimatedBuilder: bar width and color driven by the controller.
        auto bar_builder = std::make_shared<cw::AnimatedBuilder>();
        bar_builder->animation = ctrl_;
        bar_builder->builder = [ctrl = ctrl_](cw::BuildContext&) -> cw::WidgetRef
        {
            cw::CurvedAnimation curved(*ctrl, cw::Curves::easeInOut);
            const double t = curved.value();

            const float  w = cw::Tween<float> {50.0f, 280.0f}.evaluate(t);
            const cw::Color c = cw::Tween<cw::Color>{
                cw::Color::fromRGB(0.08f, 0.47f, 0.95f),
                cw::Color::fromRGB(0.95f, 0.30f, 0.10f)
            }.evaluate(t);

            auto bar = std::make_shared<cw::Container>();
            bar->width  = w;
            bar->height = 28.0f;
            bar->color  = c;
            return bar;
        };

        // AnimatedContainer: background card that grows taller when expanded.
        auto card = std::make_shared<cw::AnimatedContainer>();
        card->duration_ms = 400.0;
        card->curve       = cw::Curves::easeInOut;
        card->color       = expanded_
            ? cw::Color::fromRGB(0.88f, 0.95f, 1.0f)
            : cw::Color::fromRGB(0.93f, 0.93f, 0.97f);
        card->height      = expanded_ ? 130.0f : 80.0f;
        card->padding     = cw::EdgeInsets::all(16.0f);
        card->child = cw::make<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::make<cw::Text>("animation demo", labelStyle),
                cw::make<cw::Padding>(cw::EdgeInsets::only(0, 8, 0, 0), bar_builder),
            }
        );

        // Tap detector: toggle the animation forward/reverse and expand/collapse.
        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this]
        {
            setState([this]{ expanded_ = !expanded_; });

            if (ctrl_->status() == cw::AnimationStatus::completed ||
                ctrl_->status() == cw::AnimationStatus::forward)
                ctrl_->reverse();
            else
                ctrl_->forward();
        };
        tap->child = card;

        auto col = cw::make<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                withPointerCursor(tap),
                cw::make<cw::Padding>(cw::EdgeInsets::only(0, 6, 0, 0),
                    cw::make<cw::Text>("tap to toggle", hintStyle)),
            }
        );

        auto section_box = std::make_shared<cw::Container>();
        section_box->padding = cw::EdgeInsets::all(20.0f);
        section_box->color   = cw::Color::fromRGB(0.98f, 0.98f, 1.0f);
        section_box->child   = col;
        return section_box;
    }

private:
    std::shared_ptr<cw::AnimationController> ctrl_;
    bool expanded_ = false;
};

class AnimationSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<AnimationSectionState>();
    }
};

// ---------------------------------------------------------------------------
// ScrollSection — ListView test section
// ---------------------------------------------------------------------------

class ScrollSection : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        static const cw::Color kPalette[] = {
            cw::Color::fromRGB(0.08f, 0.47f, 0.95f),
            cw::Color::fromRGB(0.95f, 0.30f, 0.10f),
            cw::Color::fromRGB(0.10f, 0.70f, 0.40f),
            cw::Color::fromRGB(0.75f, 0.20f, 0.80f),
            cw::Color::fromRGB(0.95f, 0.65f, 0.05f),
        };

        auto headerStyle = makeStyle(13.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f));
        auto titleStyle  = makeStyle(14.0f, cw::Color::fromRGB(0.1f, 0.1f, 0.1f));
        auto subStyle    = makeStyle(11.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.5f));

        auto list = std::make_shared<cw::ListView>();
        list->item_count  = 30;
        list->item_extent = 60.0f;
        list->physics     = std::make_shared<cw::BouncingScrollPhysics>();

        list->builder = [=](cw::BuildContext&, int i) -> cw::WidgetRef
        {
            const cw::Color dot_color = kPalette[i % 5];

            // Coloured dot
            auto dot = std::make_shared<cw::Container>();
            dot->width  = 32.0f;
            dot->height = 32.0f;
            dot->color  = dot_color;

            // Text column
            auto text_col = cw::make<cw::Column>(
                cw::MainAxisAlignment::center,
                cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    cw::make<cw::Text>("Item " + std::to_string(i + 1), titleStyle),
                    cw::make<cw::Padding>(cw::EdgeInsets::only(0, 2, 0, 0),
                        cw::make<cw::Text>(
                            "Scroll me — row " + std::to_string(i + 1) + " of 30", subStyle)),
                }
            );

            auto row = cw::make<cw::Row>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    dot,
                    cw::make<cw::SizedBox>(12.0f),
                    cw::make<cw::Expanded>(text_col),
                }
            );

            auto cell = std::make_shared<cw::Container>();
            cell->padding = cw::EdgeInsets::symmetric(16.0f, 0.0f);
            cell->color   = (i % 2 == 0)
                ? cw::Color::fromRGB(1.0f, 1.0f, 1.0f)
                : cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
            cell->child   = row;
            return cell;
        };

        // Fixed-height container for the list
        auto list_box = cw::make<cw::SizedBox>(std::nullopt, 200.0f, list);

        auto section_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::make<cw::Padding>(cw::EdgeInsets::only(0, 0, 0, 6),
                    cw::make<cw::Text>("scroll list (30 items, bounce physics)", headerStyle)),
                list_box,
            }
        );

        auto section_box = std::make_shared<cw::Container>();
        section_box->padding = cw::EdgeInsets::all(16.0f);
        section_box->color   = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        section_box->child   = section_col;
        return section_box;
    }
};

// ---------------------------------------------------------------------------
// SecondScreen — second navigation route
// ---------------------------------------------------------------------------

class SecondScreen : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext& ctx) const override
    {
        auto* nav = cw::Navigator::of(ctx);

        auto title_style    = makeStyle(28.0f, cw::Color::fromRGB(0.10f, 0.10f, 0.10f));
        auto subtitle_style = makeStyle(16.0f, cw::Color::fromRGB(0.50f, 0.50f, 0.55f));
        auto btn_style      = makeStyle(15.0f, cw::Color::white());

        auto back_label = cw::make<cw::Center>(cw::make<cw::Text>("\xe2\x86\x90 Go Back", btn_style));

        auto back_box = std::make_shared<cw::Container>();
        back_box->color   = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
        back_box->padding = cw::EdgeInsets::symmetric(24.0f, 12.0f);
        back_box->child   = back_label;

        auto back_tap = std::make_shared<cw::GestureDetector>();
        back_tap->on_tap = [nav]() { if (nav) nav->pop(); };
        back_tap->child  = back_box;

        auto content = cw::make<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::make<cw::Text>("Second Screen", title_style),
                cw::make<cw::Padding>(
                    cw::EdgeInsets::only(0, 8, 0, 32),
                    cw::make<cw::Text>("You navigated here!", subtitle_style)),
                withPointerCursor(back_tap),
            }
        );

        auto bg   = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = cw::make<cw::Center>(content);
        return bg;
    }
};

// ---------------------------------------------------------------------------
// GestureApp — exercises every GestureDetector callback
// ---------------------------------------------------------------------------

class GestureApp;

class GestureAppState : public cw::State<GestureApp>
{
public:
    void initState() override
    {
        status_    = "Tap, double-tap, long-press, pan, or scroll anywhere";
        tap_count_ = 0;
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        auto* nav = cw::Navigator::of(ctx);

        // --- gesture detector ---
        auto detector = std::make_shared<cw::GestureDetector>();

        detector->on_tap = [this] {
            setState([this] {
                ++tap_count_;
                status_ = "Tap  (#" + std::to_string(tap_count_) + ")";
            });
        };

        detector->on_double_tap = [this] {
            setState([this] { status_ = "Double-tap!"; });
        };

        detector->on_long_press = [this] {
            setState([this] { status_ = "Long press!"; });
        };

        detector->on_pan_update = [this](cw::Offset delta) {
            setState([this, delta] {
                status_ = "Pan  dx=" + fmtFloat(delta.x)
                        + "  dy=" + fmtFloat(delta.y);
            });
        };

        detector->on_pan_end = [this] {
            setState([this] { status_ += "  (pan ended)"; });
        };

        detector->on_scroll = [this](cw::Offset delta) {
            setState([this, delta] {
                status_ = "Scroll  dx=" + fmtFloat(delta.x)
                        + "  dy=" + fmtFloat(delta.y);
            });
        };

        // --- UI content ---
        auto titleStyle  = makeStyle(16.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f));
        auto countStyle  = makeStyle(72.0f, cw::Color::fromRGB(0.08f, 0.47f, 0.95f));
        auto statusStyle = makeStyle(18.0f, cw::Color::fromRGB(0.15f, 0.15f, 0.15f));

        auto gesture_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::make<cw::Text>("tap count", titleStyle),
                cw::make<cw::Padding>(cw::EdgeInsets::only(0, 8, 0, 24),
                    cw::make<cw::Text>(std::to_string(tap_count_), countStyle)),
                cw::make<cw::Text>(status_, statusStyle),
            }
        );

        auto gesture_box = std::make_shared<cw::Container>();
        gesture_box->color = cw::Color::fromRGB(0.96f, 0.96f, 1.0f);
        gesture_box->child = gesture_col;
        detector->child    = gesture_box;

        // --- layout: gesture section (fills remaining space) + animation section ---
        auto expanded_gesture = std::make_shared<cw::Expanded>(
            std::make_shared<cw::Center>(detector));

        // --- navigation button bar ---
        auto nav_label = cw::make<cw::Text>(
            "Go to Second Screen \xe2\x86\x92",
            makeStyle(14.0f, cw::Color::white()));

        auto nav_box = std::make_shared<cw::Container>();
        nav_box->padding = cw::EdgeInsets::symmetric(16.0f, 10.0f);
        nav_box->child   = nav_label;

        auto nav_tap = std::make_shared<cw::GestureDetector>();
        nav_tap->on_tap = [nav]() {
            if (nav) nav->push(std::make_shared<cw::PageRoute>(
                [](cw::BuildContext&) -> cw::WidgetRef {
                    return std::make_shared<SecondScreen>();
                }));
        };
        nav_tap->child = nav_box;

        auto nav_bar = std::make_shared<cw::Container>();
        nav_bar->color   = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
        nav_bar->padding = cw::EdgeInsets::symmetric(12.0f, 6.0f);
        nav_bar->child   = cw::make<cw::Row>(
            cw::MainAxisAlignment::end,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{ withPointerCursor(nav_tap) }
        );

        auto root_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                nav_bar,
                expanded_gesture,
                cw::make<AnimationSection>(),
                cw::make<ScrollSection>(),
            }
        );

        return root_col;
    }

private:
    std::string status_;
    int         tap_count_ = 0;
};

class GestureApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<GestureAppState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::paintSizeEnabled     = true;
    cw::DebugFlags::repaintRainbowEnabled = false;
    cw::DebugFlags::showDebugBanner       = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = std::make_shared<cw::PageRoute>(
        [](cw::BuildContext&) -> cw::WidgetRef {
            return std::make_shared<GestureApp>();
        });

    return cw::runApp(
        nav,
        "campello_widgets — Gesture + Animation",
        640.0f,
        760.0f);
}
