#include <campello_widgets/widgets/animated_opacity.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/opacity.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // AnimatedOpacityState
    // ------------------------------------------------------------------

    class AnimatedOpacityState : public State<AnimatedOpacity>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            current_opacity_ = w.opacity;

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]()
            {
                setState([this]()
                {
                    const double t = curve_(ctrl_->normalizedValue());
                    current_opacity_ = lerp<float>(from_opacity_, to_opacity_, t);
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
            const auto& old_w = static_cast<const AnimatedOpacity&>(old_widget_base);
            const auto& new_w = widget();

            from_opacity_ = current_opacity_;
            to_opacity_   = new_w.opacity;

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
                        current_opacity_ = lerp<float>(from_opacity_, to_opacity_, t);
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            auto w = std::make_shared<Opacity>();
            w->opacity = current_opacity_;
            w->child   = widget().child;
            return w;
        }

    private:
        float  current_opacity_ = 1.0f;
        float  from_opacity_    = 1.0f;
        float  to_opacity_      = 1.0f;

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_ = Curves::easeInOut;
    };

    // ------------------------------------------------------------------
    // AnimatedOpacity::createState
    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> AnimatedOpacity::createState() const
    {
        return std::make_unique<AnimatedOpacityState>();
    }

} // namespace systems::leal::campello_widgets
