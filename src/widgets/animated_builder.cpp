#include <campello_widgets/widgets/animated_builder.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // AnimatedBuilderState
    // ------------------------------------------------------------------

    class AnimatedBuilderState : public State<AnimatedBuilder>
    {
    public:
        void initState() override
        {
            if (widget().animation)
            {
                listener_id_ = widget().animation->addListener([this]()
                {
                    setState([](){});
                });
            }
        }

        void dispose() override
        {
            if (widget().animation && listener_id_ != 0)
                widget().animation->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_widget) override
        {
            const auto& old_w = static_cast<const AnimatedBuilder&>(old_widget);

            // If the animation controller changed, re-subscribe.
            if (old_w.animation.get() != widget().animation.get())
            {
                if (old_w.animation && listener_id_ != 0)
                    old_w.animation->removeListener(listener_id_);

                listener_id_ = 0;

                if (widget().animation)
                {
                    listener_id_ = widget().animation->addListener([this]()
                    {
                        setState([](){});
                    });
                }
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            return widget().builder(ctx);
        }

    private:
        uint64_t listener_id_ = 0;
    };

    // ------------------------------------------------------------------
    // AnimatedBuilder::createState
    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> AnimatedBuilder::createState() const
    {
        return std::make_unique<AnimatedBuilderState>();
    }

} // namespace systems::leal::campello_widgets
