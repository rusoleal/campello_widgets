#include <campello_widgets/widgets/animated_padding.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/padding.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // AnimatedPaddingState
    // ------------------------------------------------------------------

    class AnimatedPaddingState : public State<AnimatedPadding>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            current_padding_ = w.padding;

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]()
            {
                setState([this]()
                {
                    const double t = curve_(ctrl_->normalizedValue());
                    current_padding_ = EdgeInsets{
                        lerp<float>(from_padding_.left,   to_padding_.left,   t),
                        lerp<float>(from_padding_.top,    to_padding_.top,    t),
                        lerp<float>(from_padding_.right,  to_padding_.right,  t),
                        lerp<float>(from_padding_.bottom, to_padding_.bottom, t),
                    };
                });
            });
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_widget_base) override
        {
            const auto& old_w = static_cast<const AnimatedPadding&>(old_widget_base);
            const auto& new_w = widget();

            from_padding_ = current_padding_;
            to_padding_   = new_w.padding;

            if (old_w.duration_ms != new_w.duration_ms)
            {
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);

                ctrl_ = std::make_unique<AnimationController>(new_w.duration_ms);
                listener_id_ = ctrl_->addListener([this]()
                {
                    setState([this]()
                    {
                        const double t = curve_(ctrl_->normalizedValue());
                        current_padding_ = EdgeInsets{
                            lerp<float>(from_padding_.left,   to_padding_.left,   t),
                            lerp<float>(from_padding_.top,    to_padding_.top,    t),
                            lerp<float>(from_padding_.right,  to_padding_.right,  t),
                            lerp<float>(from_padding_.bottom, to_padding_.bottom, t),
                        };
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            auto w = std::make_shared<Padding>();
            w->padding = current_padding_;
            w->child   = widget().child;
            return w;
        }

    private:
        EdgeInsets current_padding_;
        EdgeInsets from_padding_, to_padding_;

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_ = Curves::easeInOut;
    };

    // ------------------------------------------------------------------
    // AnimatedPadding::createState
    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> AnimatedPadding::createState() const
    {
        return std::make_unique<AnimatedPaddingState>();
    }

} // namespace systems::leal::campello_widgets
