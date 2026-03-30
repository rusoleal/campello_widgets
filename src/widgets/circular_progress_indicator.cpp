#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // Painter
    // ------------------------------------------------------------------

    class CircularProgressPainter : public CustomPainter
    {
    public:
        std::optional<float> value;
        double               anim_t        = 0.0;
        Color                background_color = Color::transparent();
        Color                value_color      = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        float                stroke_width     = 4.0f;

        void paint(Canvas& canvas, Size size) override
        {
            static constexpr float kPi       = 3.14159265358979f;
            static constexpr float kTwoPi    = 2.0f * kPi;
            static constexpr float kHalfPi   = kPi * 0.5f;

            const float cx      = size.width  * 0.5f;
            const float cy      = size.height * 0.5f;
            const float radius  = cx - stroke_width * 0.5f;
            const Rect  bounds  = Rect::fromLTWH(
                cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

            Paint stroke = Paint::stroked(value_color, stroke_width);

            // Background ring
            if (background_color.a > 0.0f) {
                Paint bg_stroke = Paint::stroked(background_color, stroke_width);
                canvas.drawArc(bounds, 0.0f, kTwoPi, false, bg_stroke);
            }

            if (value.has_value()) {
                // Determinate: arc starts at top (-π/2) and sweeps clockwise
                const float sweep = kTwoPi * std::clamp(*value, 0.0f, 1.0f);
                if (sweep > 0.0f)
                    canvas.drawArc(bounds, -kHalfPi, sweep, false, stroke);
            } else {
                // Indeterminate: 270° arc rotates continuously
                static constexpr float kSweep = kTwoPi * 0.75f; // 270°
                const float start = static_cast<float>(anim_t) * kTwoPi - kHalfPi;
                canvas.drawArc(bounds, start, kSweep, false, stroke);
            }
        }

        bool shouldRepaint(const CustomPainter&) const override { return true; }
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    class CircularProgressIndicatorState : public State<CircularProgressIndicator>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            if (!w.value.has_value())
                startAnimation(w.duration_ms);
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const CircularProgressIndicator&>(old_base);
            const auto& new_w = widget();

            const bool was_indet = !old_w.value.has_value();
            const bool is_indet  = !new_w.value.has_value();

            if (was_indet && !is_indet) {
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);
                ctrl_.reset();
                listener_id_ = 0;
                anim_t_      = 0.0;
            } else if (!was_indet && is_indet) {
                startAnimation(new_w.duration_ms);
            }
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto painter              = std::make_shared<CircularProgressPainter>();
            painter->value            = w.value;
            painter->anim_t           = anim_t_;
            painter->background_color = w.background_color;
            painter->value_color      = w.value_color;
            painter->stroke_width     = w.stroke_width;

            auto box    = std::make_shared<SizedBox>();
            box->width  = w.size;
            box->height = w.size;
            box->child  = RawCustomPaint::create(painter);
            return box;
        }

    private:
        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        double                               anim_t_      = 0.0;

        void startAnimation(double dur_ms)
        {
            ctrl_ = std::make_unique<AnimationController>(dur_ms);
            listener_id_ = ctrl_->addListener([this]() {
                setState([this]() {
                    anim_t_ = ctrl_->normalizedValue();
                    if (ctrl_->normalizedValue() >= 1.0)
                        ctrl_->forward(0.0); // loop
                });
            });
            ctrl_->forward(0.0);
        }
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> CircularProgressIndicator::createState() const
    {
        return std::make_unique<CircularProgressIndicatorState>();
    }

} // namespace systems::leal::campello_widgets
