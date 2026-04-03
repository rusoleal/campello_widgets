#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Rebuilds a subtree on every animation tick.
     *
     * AnimatedBuilder subscribes to an AnimationController and calls `builder`
     * each time the controller's value changes, triggering a setState-driven
     * rebuild of only this widget's subtree.
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(400.0);
     * ctrl->forward();
     *
     * auto widget = std::make_shared<AnimatedBuilder>();
     * widget->animation = ctrl;
     * widget->builder = [ctrl](BuildContext&) {
     *     return std::make_shared<Container>(
     *         Container{.width  = Tween<float>{0.f, 200.f}.evaluate(*ctrl),
     *                   .height = 50.f,
     *                   .color  = Color::blue()});
     * };
     * @endcode
     */
    class AnimatedBuilder : public StatefulWidget
    {
    public:
        std::shared_ptr<AnimationController>    animation;
        std::function<WidgetRef(BuildContext&)> builder;

        AnimatedBuilder() = default;
        explicit AnimatedBuilder(
            std::shared_ptr<AnimationController> ctrl,
            std::function<WidgetRef(BuildContext&)> b)
            : animation(std::move(ctrl)), builder(std::move(b))
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
