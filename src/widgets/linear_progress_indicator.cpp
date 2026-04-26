#include <campello_widgets/widgets/linear_progress_indicator.hpp>
#include <campello_widgets/widgets/theme.hpp>
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

    class LinearProgressPainter : public CustomPainter
    {
    public:
        std::optional<float> value;
        double               anim_t        = 0.0;
        Color                background    = Color::fromRGBA(0.051f, 0.545f, 0.553f, 0.24f);
        Color                value_color   = Color::fromRGBA(0.051f, 0.545f, 0.553f, 1.0f);

        void paint(Canvas& canvas, Size size) override
        {
            // Background
            canvas.drawRect(
                Rect::fromLTWH(0.0f, 0.0f, size.width, size.height),
                Paint::filled(background));

            if (value.has_value()) {
                // Determinate
                const float w = size.width * std::clamp(*value, 0.0f, 1.0f);
                if (w > 0.0f)
                    canvas.drawRect(
                        Rect::fromLTWH(0.0f, 0.0f, w, size.height),
                        Paint::filled(value_color));
            } else {
                // Indeterminate: a 40 %-wide bar slides left→right and wraps
                static constexpr float kBarFrac = 0.4f;
                const float t   = static_cast<float>(anim_t);
                // bar left edge travels from -kBarFrac to 1.0, then wraps
                const float x0  = (t - kBarFrac) * size.width;
                const float x1  = t * size.width;
                const float cx0 = std::max(0.0f, x0);
                const float cx1 = std::min(size.width, x1);
                if (cx1 > cx0)
                    canvas.drawRect(
                        Rect::fromLTWH(cx0, 0.0f, cx1 - cx0, size.height),
                        Paint::filled(value_color));
            }
        }

        bool shouldRepaint(const CustomPainter&) const override { return true; }
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    class LinearProgressIndicatorState : public State<LinearProgressIndicator>
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
            const auto& old_w = static_cast<const LinearProgressIndicator&>(old_base);
            const auto& new_w = widget();

            const bool was_indet = !old_w.value.has_value();
            const bool is_indet  = !new_w.value.has_value();

            if (was_indet && !is_indet) {
                // Became determinate — stop animation
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);
                ctrl_.reset();
                listener_id_ = 0;
                anim_t_      = 0.0;
            } else if (!was_indet && is_indet) {
                startAnimation(new_w.duration_ms);
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(ctx);

            auto painter            = std::make_shared<LinearProgressPainter>();
            painter->value          = w.value;
            painter->anim_t         = anim_t_;
            painter->background     = w.background_color.value_or(
                Color::fromRGBA(tokens->colors.primary.r, tokens->colors.primary.g, tokens->colors.primary.b, tokens->colors.primary.a * 0.15f));
            painter->value_color    = w.value_color.value_or(tokens->colors.primary);

            auto box    = std::make_shared<SizedBox>();
            box->height = w.min_height;
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
                    // Map normalizedValue (0→1) to bar position (0→1.4→0→...)
                    // We want the bar to slide continuously: use a sawtooth pattern
                    const double v = ctrl_->normalizedValue();
                    anim_t_ = v * 1.4; // 0 to 1.4 (bar travels 1.4 widths per cycle)
                    if (ctrl_->normalizedValue() >= 1.0)
                        ctrl_->forward(0.0); // loop
                });
            });
            ctrl_->forward(0.0);
        }
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> LinearProgressIndicator::createState() const
    {
        return std::make_unique<LinearProgressIndicatorState>();
    }

} // namespace systems::leal::campello_widgets
