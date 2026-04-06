#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

static constexpr std::size_t kMaxLog = 6;

// ---------------------------------------------------------------------------
// GesturesApp
// ---------------------------------------------------------------------------

class GesturesApp;

class GesturesState : public cw::State<GesturesApp>
{
public:
    void initState() override
    {
        last_gesture_       = "interact with the zone";
        zone_color_         = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);
        gesture_text_color_ = cw::Color::fromRGB(0.40f, 0.40f, 0.45f);
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        // ---- Styles ----
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 15.0f;
        titleStyle.color       = cw::Color::fromRGB(0.15f, 0.15f, 0.20f);

        cw::TextStyle subtitleStyle{};
        subtitleStyle.font_family = "Helvetica Neue";
        subtitleStyle.font_size   = 12.0f;
        subtitleStyle.color       = cw::Color::fromRGB(0.55f, 0.55f, 0.60f);

        cw::TextStyle gestureStyle{};
        gestureStyle.font_family = "Helvetica Neue";
        gestureStyle.font_size   = 36.0f;
        gestureStyle.color       = gesture_text_color_;

        cw::TextStyle detailStyle{};
        detailStyle.font_family = "Helvetica Neue";
        detailStyle.font_size   = 13.0f;
        detailStyle.color       = cw::Color::fromRGB(0.90f, 0.90f, 1.0f);

        cw::TextStyle logLabelStyle{};
        logLabelStyle.font_family = "Helvetica Neue";
        logLabelStyle.font_size   = 11.0f;
        logLabelStyle.color       = cw::Color::fromRGB(0.55f, 0.55f, 0.60f);

        cw::TextStyle logStyle{};
        logStyle.font_family = "Helvetica Neue";
        logStyle.font_size   = 11.0f;
        logStyle.color       = cw::Color::fromRGB(0.55f, 0.55f, 0.60f);

        // ---- Gesture zone interior ----
        auto zone_col = std::make_shared<cw::Column>();
        zone_col->main_axis_alignment  = cw::MainAxisAlignment::center;
        zone_col->cross_axis_alignment = cw::CrossAxisAlignment::center;
        zone_col->main_axis_size       = cw::MainAxisSize::min;
        zone_col->children = {
            cw::mw<cw::Text>(last_gesture_, gestureStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
            cw::mw<cw::Text>(detail_, detailStyle),
        };

        auto zone_bg = std::make_shared<cw::Container>();
        zone_bg->color = zone_color_;
        zone_bg->child = cw::mw<cw::Center>(zone_col);

        auto detector = std::make_shared<cw::GestureDetector>();
        detector->child = zone_bg;

        detector->on_tap = [this] {
            setState([this] {
                last_gesture_       = "tap";
                detail_             = "";
                zone_color_         = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
                gesture_text_color_ = cw::Color::white();
                pushLog("tap");
            });
        };

        detector->on_double_tap = [this] {
            setState([this] {
                last_gesture_       = "double tap";
                detail_             = "";
                zone_color_         = cw::Color::fromRGB(0.10f, 0.70f, 0.40f);
                gesture_text_color_ = cw::Color::white();
                pushLog("double tap");
            });
        };

        detector->on_long_press = [this] {
            setState([this] {
                last_gesture_       = "long press";
                detail_             = "";
                zone_color_         = cw::Color::fromRGB(0.90f, 0.45f, 0.10f);
                gesture_text_color_ = cw::Color::white();
                pushLog("long press");
            });
        };

        detector->on_pan_update = [this](cw::Offset delta) {
            setState([this, delta] {
                pan_acc_.x         += delta.x;
                pan_acc_.y         += delta.y;
                last_gesture_       = "pan";
                detail_             = fmtOffset("delta", delta) + "   total " + fmtOffset("", pan_acc_);
                zone_color_         = cw::Color::fromRGB(0.45f, 0.20f, 0.80f);
                gesture_text_color_ = cw::Color::white();
                pushLog("pan_update  " + fmtOffset("", delta));
            });
        };

        detector->on_pan_end = [this] {
            setState([this] {
                pan_acc_            = cw::Offset::zero();
                last_gesture_       = "pan end";
                detail_             = "";
                zone_color_         = cw::Color::fromRGB(0.60f, 0.40f, 0.85f);
                gesture_text_color_ = cw::Color::white();
                pushLog("pan_end");
            });
        };

        detector->on_scroll = [this](cw::Offset delta) {
            setState([this, delta] {
                scroll_acc_.x      += delta.x;
                scroll_acc_.y      += delta.y;
                last_gesture_       = "scroll";
                detail_             = fmtOffset("delta", delta) + "   total " + fmtOffset("", scroll_acc_);
                zone_color_         = cw::Color::fromRGB(0.05f, 0.65f, 0.75f);
                gesture_text_color_ = cw::Color::white();
                pushLog("scroll  " + fmtOffset("", delta));
            });
        };

        // ---- Event log ----
        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it)
            log_items.push_back(cw::mw<cw::Text>(*it, logStyle));

        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = std::move(log_items);

        auto log_section = std::make_shared<cw::Column>();
        log_section->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_section->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_section->main_axis_size       = cw::MainAxisSize::min;
        log_section->children = {
            cw::mw<cw::Text>("event log", logLabelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 6.0f),
            log_col,
        };

        // ---- Header ----
        auto header_col = std::make_shared<cw::Column>();
        header_col->main_axis_alignment  = cw::MainAxisAlignment::center;
        header_col->cross_axis_alignment = cw::CrossAxisAlignment::center;
        header_col->main_axis_size       = cw::MainAxisSize::min;
        header_col->children = {
            cw::mw<cw::Text>("GestureDetector", titleStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 4.0f),
            cw::mw<cw::Text>("tap · double-tap · long-press · pan · scroll", subtitleStyle),
        };

        // ---- Root ----
        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 32.0f, 0.0f, 16.0f),
                    cw::mw<cw::Center>(header_col)),
                cw::mw<cw::Expanded>(detector),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(32.0f, 16.0f, 32.0f, 32.0f),
                    log_section),
            });

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root;
        return bg;
    }

private:
    static std::string fmtOffset(const std::string& label, cw::Offset o)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        if (!label.empty()) ss << label << " ";
        ss << (o.x >= 0 ? "+" : "") << o.x
           << ", "
           << (o.y >= 0 ? "+" : "") << o.y;
        return ss.str();
    }

    void pushLog(std::string entry)
    {
        log_.push_back(std::move(entry));
        if (log_.size() > kMaxLog)
            log_.erase(log_.begin());
    }

    std::string last_gesture_;
    std::string detail_;
    cw::Color   zone_color_;
    cw::Color   gesture_text_color_;
    cw::Offset  pan_acc_;
    cw::Offset  scroll_acc_;
    std::vector<std::string> log_;
};

class GesturesApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<GesturesState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner = false;

    return cw::runApp(
        std::make_shared<GesturesApp>(),
        "campello_widgets — GestureDetector",
        480.0f,
        560.0f);
}
