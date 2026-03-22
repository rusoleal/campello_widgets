#include <campello_widgets/widgets/animated_container.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/container.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // AnimatedContainerState
    // ------------------------------------------------------------------

    class AnimatedContainerState : public State<AnimatedContainer>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            // Initialise current values to the widget's initial configuration.
            current_color_   = w.color;
            current_width_   = w.width;
            current_height_  = w.height;
            current_padding_ = w.padding;

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]()
            {
                setState([this]()
                {
                    const double t = curve_(ctrl_->normalizedValue());
                    interpolate(t);
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
            const auto& old_w = static_cast<const AnimatedContainer&>(old_widget_base);
            const auto& new_w = widget();

            // Save current animated values as the "from" snapshot.
            from_color_   = current_color_;
            from_width_   = current_width_;
            from_height_  = current_height_;
            from_padding_ = current_padding_;

            // Target values from the new widget.
            to_color_   = new_w.color;
            to_width_   = new_w.width;
            to_height_  = new_w.height;
            to_padding_ = new_w.padding;

            // Re-create the controller if the duration changed.
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
                        interpolate(t);
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            auto c = std::make_shared<Container>();
            c->color     = current_color_;
            c->width     = current_width_;
            c->height    = current_height_;
            c->padding   = current_padding_;
            c->alignment = widget().alignment;
            c->child     = widget().child;
            return c;
        }

    private:
        void interpolate(double t)
        {
            if (from_color_ && to_color_)
                current_color_ = lerp<Color>(*from_color_, *to_color_, t);
            else
                current_color_ = to_color_;

            if (from_width_ && to_width_)
                current_width_ = lerp<float>(*from_width_, *to_width_, t);
            else
                current_width_ = to_width_;

            if (from_height_ && to_height_)
                current_height_ = lerp<float>(*from_height_, *to_height_, t);
            else
                current_height_ = to_height_;

            if (from_padding_ && to_padding_)
            {
                current_padding_ = EdgeInsets{
                    lerp<float>(from_padding_->left,   to_padding_->left,   t),
                    lerp<float>(from_padding_->top,    to_padding_->top,    t),
                    lerp<float>(from_padding_->right,  to_padding_->right,  t),
                    lerp<float>(from_padding_->bottom, to_padding_->bottom, t),
                };
            }
            else
            {
                current_padding_ = to_padding_;
            }
        }

        // Current animated values (what the Container is built with).
        std::optional<Color>      current_color_;
        std::optional<float>      current_width_;
        std::optional<float>      current_height_;
        std::optional<EdgeInsets> current_padding_;

        // Transition endpoints.
        std::optional<Color>      from_color_,   to_color_;
        std::optional<float>      from_width_,   to_width_;
        std::optional<float>      from_height_,  to_height_;
        std::optional<EdgeInsets> from_padding_, to_padding_;

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_ = Curves::easeInOut;
    };

    // ------------------------------------------------------------------
    // AnimatedContainer::createState
    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> AnimatedContainer::createState() const
    {
        return std::make_unique<AnimatedContainerState>();
    }

} // namespace systems::leal::campello_widgets
