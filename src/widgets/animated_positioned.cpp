#include <campello_widgets/widgets/animated_positioned.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/positioned.hpp>

#include <optional>

namespace systems::leal::campello_widgets
{

    // Lerp two optional<float> values.
    // Both present → lerp; one or both absent → keep new_val at end of animation.
    static float lerpOptVal(std::optional<float> a, std::optional<float> b, double t)
    {
        if (a.has_value() && b.has_value())
            return lerp<float>(*a, *b, static_cast<double>(t));
        return b.value_or(0.0f);
    }

    class AnimatedPositionedState : public State<AnimatedPositioned>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            snap(w);

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]() {
                setState([this]() {
                    const double t = curve_(ctrl_->normalizedValue());
                    cur_left_   = optLerp(from_left_,   to_left_,   t);
                    cur_top_    = optLerp(from_top_,    to_top_,    t);
                    cur_right_  = optLerp(from_right_,  to_right_,  t);
                    cur_bottom_ = optLerp(from_bottom_,  to_bottom_, t);
                    cur_width_  = optLerp(from_width_,  to_width_,  t);
                    cur_height_ = optLerp(from_height_, to_height_, t);
                });
            });
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const AnimatedPositioned&>(old_base);
            const auto& new_w = widget();

            const bool changed =
                old_w.left != new_w.left || old_w.top    != new_w.top    ||
                old_w.right != new_w.right || old_w.bottom != new_w.bottom ||
                old_w.width != new_w.width || old_w.height != new_w.height;

            if (!changed) return;

            // Snapshot current animated position as start
            from_left_   = cur_left_;
            from_top_    = cur_top_;
            from_right_  = cur_right_;
            from_bottom_ = cur_bottom_;
            from_width_  = cur_width_;
            from_height_ = cur_height_;

            // New target
            to_left_   = new_w.left;
            to_top_    = new_w.top;
            to_right_  = new_w.right;
            to_bottom_ = new_w.bottom;
            to_width_  = new_w.width;
            to_height_ = new_w.height;

            if (old_w.duration_ms != new_w.duration_ms) {
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);
                ctrl_ = std::make_unique<AnimationController>(new_w.duration_ms);
                listener_id_ = ctrl_->addListener([this]() {
                    setState([this]() {
                        const double t = curve_(ctrl_->normalizedValue());
                        cur_left_   = optLerp(from_left_,   to_left_,   t);
                        cur_top_    = optLerp(from_top_,    to_top_,    t);
                        cur_right_  = optLerp(from_right_,  to_right_,  t);
                        cur_bottom_ = optLerp(from_bottom_, to_bottom_, t);
                        cur_width_  = optLerp(from_width_,  to_width_,  t);
                        cur_height_ = optLerp(from_height_, to_height_, t);
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            auto p    = std::make_shared<Positioned>();
            p->left   = cur_left_;
            p->top    = cur_top_;
            p->right  = cur_right_;
            p->bottom = cur_bottom_;
            p->width  = cur_width_;
            p->height = cur_height_;
            p->child  = widget().child;
            return p;
        }

    private:
        // Current animated values
        std::optional<float> cur_left_, cur_top_, cur_right_,
                             cur_bottom_, cur_width_, cur_height_;
        // Start values
        std::optional<float> from_left_, from_top_, from_right_,
                             from_bottom_, from_width_, from_height_;
        // Target values
        std::optional<float> to_left_, to_top_, to_right_,
                             to_bottom_, to_width_, to_height_;

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_       = Curves::easeInOut;

        void snap(const AnimatedPositioned& w)
        {
            cur_left_   = to_left_   = from_left_   = w.left;
            cur_top_    = to_top_    = from_top_    = w.top;
            cur_right_  = to_right_  = from_right_  = w.right;
            cur_bottom_ = to_bottom_ = from_bottom_ = w.bottom;
            cur_width_  = to_width_  = from_width_  = w.width;
            cur_height_ = to_height_ = from_height_ = w.height;
        }

        // Lerp two optional<float>; if both present interpolate, else take target at t==1
        static std::optional<float> optLerp(
            std::optional<float> a, std::optional<float> b, double t)
        {
            if (!b.has_value()) return std::nullopt;
            if (!a.has_value()) return b;
            return lerp<float>(*a, *b, t);
        }
    };

    std::unique_ptr<StateBase> AnimatedPositioned::createState() const
    {
        return std::make_unique<AnimatedPositionedState>();
    }

} // namespace systems::leal::campello_widgets
