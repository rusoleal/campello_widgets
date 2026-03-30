#include <campello_widgets/widgets/fade_transition.hpp>
#include <campello_widgets/widgets/opacity.hpp>

namespace systems::leal::campello_widgets
{

    class FadeTransitionState : public State<FadeTransition>
    {
    public:
        void initState() override
        {
            attach(widget().controller);
        }

        void dispose() override
        {
            detach(ctrl_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const FadeTransition&>(old_base);
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
            auto o = std::make_shared<Opacity>();
            o->opacity = w.opacity.evaluate(t);
            o->child   = w.child;
            return o;
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

    std::unique_ptr<StateBase> FadeTransition::createState() const
    {
        return std::make_unique<FadeTransitionState>();
    }

} // namespace systems::leal::campello_widgets
