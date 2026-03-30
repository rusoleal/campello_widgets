#include <campello_widgets/widgets/scale_transition.hpp>
#include <campello_widgets/widgets/transform.hpp>

namespace systems::leal::campello_widgets
{

    class ScaleTransitionState : public State<ScaleTransition>
    {
    public:
        void initState() override    { attach(widget().controller); }
        void dispose()   override    { detach(ctrl_); }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const ScaleTransition&>(old_base);
            const auto& new_w = widget();
            if (old_w.controller != new_w.controller) {
                detach(ctrl_);
                attach(new_w.controller);
            }
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();
            double t = 0.0;
            if (ctrl_) {
                const double raw = ctrl_->normalizedValue();
                t = w.curve ? w.curve(raw) : raw;
            }
            const float s = w.scale.evaluate(t);

            auto tr        = std::make_shared<Transform>();
            tr->transform  = Transform::scaling(s);
            tr->alignment  = w.alignment;
            tr->child      = w.child;
            return tr;
        }

    private:
        std::shared_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;

        void attach(const std::shared_ptr<AnimationController>& c)
        {
            ctrl_ = c;
            if (ctrl_)
                listener_id_ = ctrl_->addListener([this]() { setState([]{}); });
        }

        void detach(const std::shared_ptr<AnimationController>& c)
        {
            if (c && listener_id_ != 0)
                c->removeListener(listener_id_);
            listener_id_ = 0;
        }
    };

    std::unique_ptr<StateBase> ScaleTransition::createState() const
    {
        return std::make_unique<ScaleTransitionState>();
    }

} // namespace systems::leal::campello_widgets
