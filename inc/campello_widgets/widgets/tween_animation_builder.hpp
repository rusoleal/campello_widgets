#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/tween.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    // Forward declaration
    template<typename T> class TweenAnimationBuilder;

    /**
     * @brief State for TweenAnimationBuilder<T>.
     *
     * Same implicit-animation shape as AnimatedOpacityState: whenever the
     * widget's `value` changes across an update, animates from the previous
     * value to the new one over `duration_ms`, rebuilding via `builder` on
     * every tick.
     */
    template<typename T>
    class TweenAnimationBuilderState : public State<TweenAnimationBuilder<T>>
    {
    public:
        void initState() override
        {
            const auto& w = this->widget();
            current_value_ = w.value;
            from_value_    = w.value;
            to_value_      = w.value;

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]()
            {
                this->setState([this]()
                {
                    const double t = curve_(ctrl_->normalizedValue());
                    current_value_ = lerp<T>(from_value_, to_value_, t);
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
            const auto& old_w = static_cast<const TweenAnimationBuilder<T>&>(old_widget_base);
            const auto& new_w = this->widget();

            from_value_ = current_value_;
            to_value_   = new_w.value;

            if (old_w.duration_ms != new_w.duration_ms)
            {
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);

                ctrl_ = std::make_unique<AnimationController>(new_w.duration_ms);
                listener_id_ = ctrl_->addListener([this]()
                {
                    this->setState([this]()
                    {
                        const double t = curve_(ctrl_->normalizedValue());
                        current_value_ = lerp<T>(from_value_, to_value_, t);
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = this->widget();
            return w.builder(ctx, current_value_, w.child);
        }

    private:
        T current_value_{};
        T from_value_{};
        T to_value_{};

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_ = Curves::easeInOut;
    };

    /**
     * @brief Animates a value from its previous state to a new one whenever
     *        that new value is supplied, rebuilding via `builder` each tick.
     *
     * Mirrors Flutter's `TweenAnimationBuilder<T>`. Unlike the implicit
     * `Animated*` widgets (which animate a fixed set of well-known
     * properties), this drives an arbitrary caller-supplied value of any
     * type with a `lerp<T>` specialization (see `tween.hpp`) through a
     * caller-supplied `builder`.
     *
     * @code
     * auto w = std::make_shared<TweenAnimationBuilder<float>>();
     * w->value       = expanded ? 200.0f : 48.0f;
     * w->duration_ms = 250.0;
     * w->builder     = [](BuildContext&, const float& v, WidgetRef child) {
     *     return SizedBox::from_width(v, child);
     * };
     * w->child = someChild;
     * @endcode
     */
    template<typename T>
    class TweenAnimationBuilder : public StatefulWidget
    {
    public:
        T value{};
        WidgetRef child;

        double                                                        duration_ms = 300.0;
        std::function<double(double)>                                 curve       = Curves::easeInOut;
        std::function<WidgetRef(BuildContext&, const T&, WidgetRef)>  builder;

        TweenAnimationBuilder() = default;
        explicit TweenAnimationBuilder(
            T v,
            std::function<WidgetRef(BuildContext&, const T&, WidgetRef)> b,
            WidgetRef c = nullptr)
            : value(std::move(v)), child(std::move(c)), builder(std::move(b))
        {
        }

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<TweenAnimationBuilderState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
