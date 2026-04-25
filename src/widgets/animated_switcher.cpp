#include <campello_widgets/widgets/animated_switcher.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>

namespace systems::leal::campello_widgets
{

    class AnimatedSwitcherState : public State<AnimatedSwitcher>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            current_child_ = w.child;

            in_ctrl_  = std::make_unique<AnimationController>(w.duration_ms);
            out_ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            curve_    = w.curve ? w.curve : Curves::easeInOut;

            in_listener_  = in_ctrl_->addListener([this]() {
                setState([this]() {
                    if (in_ctrl_->normalizedValue() >= 1.0 &&
                        out_ctrl_->normalizedValue() >= 1.0) {
                        prev_child_    = nullptr;
                        transitioning_ = false;
                    }
                });
            });
            out_listener_ = out_ctrl_->addListener([this]() {
                setState([this]() {
                    if (out_ctrl_->normalizedValue() >= 1.0 &&
                        in_ctrl_->normalizedValue() >= 1.0) {
                        prev_child_    = nullptr;
                        transitioning_ = false;
                    }
                });
            });
        }

        void dispose() override
        {
            if (in_ctrl_  && in_listener_  != 0) in_ctrl_->removeListener(in_listener_);
            if (out_ctrl_ && out_listener_ != 0) out_ctrl_->removeListener(out_listener_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const AnimatedSwitcher&>(old_base);
            const auto& new_w = widget();

            if (old_w.child == new_w.child) return;

            // New child arrived — start transition.
            prev_child_    = current_child_;
            current_child_ = new_w.child;
            transitioning_ = true;

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;

            in_ctrl_->forward(0.0);
            out_ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            if (!transitioning_ || !prev_child_)
                return current_child_;

            // Compute opacities from normalized controller values
            const double in_t  = curve_(in_ctrl_->normalizedValue());
            const double out_t = curve_(out_ctrl_->normalizedValue());

            const float in_opacity  = static_cast<float>(in_t);
            const float out_opacity = 1.0f - static_cast<float>(out_t);

            auto out_wrap     = std::make_shared<Opacity>();
            out_wrap->opacity = out_opacity;
            out_wrap->child   = prev_child_;

            auto in_wrap      = std::make_shared<Opacity>();
            in_wrap->opacity  = in_opacity;
            in_wrap->child    = current_child_;

            return std::make_shared<Stack>(WidgetList{out_wrap, in_wrap});
        }

    private:
        WidgetRef current_child_;
        WidgetRef prev_child_;
        bool      transitioning_ = false;

        std::unique_ptr<AnimationController> in_ctrl_;
        std::unique_ptr<AnimationController> out_ctrl_;
        uint64_t                             in_listener_  = 0;
        uint64_t                             out_listener_ = 0;
        std::function<double(double)>        curve_        = Curves::easeInOut;
    };

    std::unique_ptr<StateBase> AnimatedSwitcher::createState() const
    {
        return std::make_unique<AnimatedSwitcherState>();
    }

} // namespace systems::leal::campello_widgets
