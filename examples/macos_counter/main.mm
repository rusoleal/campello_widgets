#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Button — a pressable rectangle with a label
// ---------------------------------------------------------------------------

class Button;

class ButtonState : public cw::State<Button>
{
public:
    cw::WidgetRef build(cw::BuildContext&) override;

    bool pressed_ = false;
};

class Button : public cw::StatefulWidget
{
public:
    std::string  label;
    cw::Color    color    = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
    float        width    = 64.0f;
    float        height   = 64.0f;
    std::function<void()> on_press;

    explicit Button(std::string lbl,
                    cw::Color   c,
                    float       w,
                    float       h,
                    std::function<void()> fn)
        : label(std::move(lbl)), color(c), width(w), height(h), on_press(std::move(fn))
    {}

    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ButtonState>();
    }
};

cw::WidgetRef ButtonState::build(cw::BuildContext&)
{
    const auto& btn = widget();

    cw::TextStyle labelStyle{};
    labelStyle.font_family = "Helvetica Neue";
    labelStyle.font_size   = 28.0f;
    labelStyle.color       = cw::Color::white();

    const cw::Color bg = pressed_
        ? cw::Color::fromRGB(btn.color.r * 0.75f, btn.color.g * 0.75f, btn.color.b * 0.75f)
        : btn.color;

    auto box = std::make_shared<cw::Container>();
    box->width   = btn.width;
    box->height  = btn.height;
    box->color   = bg;
    box->child   = cw::make<cw::Center>(cw::make<cw::Text>(btn.label, labelStyle));

    auto tap = std::make_shared<cw::GestureDetector>();
    tap->on_tap = btn.on_press;
    tap->on_pan_update = [this](cw::Offset) {};
    tap->child = box;

    // Track press state for visual feedback
    auto detector = std::make_shared<cw::GestureDetector>();
    detector->on_pan_update = [this](cw::Offset) {
        setState([this] { pressed_ = true; });
    };
    detector->on_pan_end = [this] {
        setState([this] { pressed_ = false; });
    };
    detector->on_tap = [this] {
        setState([this] { pressed_ = false; });
        if (widget().on_press) widget().on_press();
    };
    detector->child = box;

    return detector;
}

// ---------------------------------------------------------------------------
// CounterApp
// ---------------------------------------------------------------------------

class CounterApp;

class CounterState : public cw::State<CounterApp>
{
public:
    void initState() override { count_ = 0; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 16.0f;
        titleStyle.color       = cw::Color::fromRGB(0.5f, 0.5f, 0.55f);

        cw::TextStyle countStyle{};
        countStyle.font_family = "Helvetica Neue";
        countStyle.font_size   = 96.0f;
        countStyle.color       = count_ >= 0
            ? cw::Color::fromRGB(0.08f, 0.47f, 0.95f)
            : cw::Color::fromRGB(0.85f, 0.20f, 0.15f);

        cw::TextStyle hintStyle{};
        hintStyle.font_family = "Helvetica Neue";
        hintStyle.font_size   = 13.0f;
        hintStyle.color       = cw::Color::fromRGB(0.65f, 0.65f, 0.65f);

        // Counter display
        auto counter_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::make<cw::Text>("tap count", titleStyle),
                cw::make<cw::Padding>(
                    cw::EdgeInsets::symmetric(0.0f, 16.0f),
                    cw::make<cw::Text>(std::to_string(count_), countStyle)),
                cw::make<cw::Text>(count_ == 0 ? "press + to start"
                                  : count_ == 1 ? "1 press so far"
                                  : std::to_string(count_) + " presses so far",
                                  hintStyle),
            }
        );

        // Buttons row — decrement, reset, increment
        const cw::Color dec_color = cw::Color::fromRGB(0.85f, 0.20f, 0.15f);
        const cw::Color rst_color = cw::Color::fromRGB(0.50f, 0.50f, 0.55f);
        const cw::Color inc_color = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);

        auto dec_btn = cw::make<Button>("-", dec_color, 64.0f, 64.0f,
            [this] { setState([this] { --count_; }); });

        auto rst_btn = cw::make<Button>("0", rst_color, 64.0f, 64.0f,
            [this] { setState([this] { count_ = 0; }); });

        auto inc_btn = cw::make<Button>("+", inc_color, 64.0f, 64.0f,
            [this] { setState([this] { ++count_; }); });

        auto buttons_row = cw::make<cw::Row>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                dec_btn,
                cw::make<cw::SizedBox>(24.0f),
                rst_btn,
                cw::make<cw::SizedBox>(24.0f),
                inc_btn,
            }
        );

        auto root_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::make<cw::Expanded>(
                    cw::make<cw::Center>(counter_col)),
                cw::make<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 0.0f, 0.0f, 40.0f),
                    buttons_row),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root_col;
        return bg;
    }

private:
    int count_ = 0;
};

class CounterApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<CounterState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = false;

    return cw::runApp(
        std::make_shared<CounterApp>(),
        "campello_widgets — Counter",
        400.0f,
        500.0f);
}
