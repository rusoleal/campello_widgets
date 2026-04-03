#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Explicitly animates the opacity of a child using an external
     * AnimationController.
     *
     * Unlike `AnimatedOpacity` (implicit — triggers on property change),
     * `FadeTransition` is driven entirely by the caller's controller.
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(400.0);
     * ctrl->forward();
     *
     * auto w = std::make_shared<FadeTransition>();
     * w->controller = ctrl;
     * w->opacity    = {0.0f, 1.0f};   // optional — default is 0 → 1
     * w->child      = someWidget;
     * @endcode
     */
    class FadeTransition : public StatefulWidget
    {
    public:
        std::shared_ptr<AnimationController> controller;
        std::function<double(double)>        curve   = Curves::linear;
        Tween<float>                         opacity = {0.0f, 1.0f};
        WidgetRef                            child;

        FadeTransition() = default;
        explicit FadeTransition(
            std::shared_ptr<AnimationController> ctrl,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), child(std::move(c))
        {}
        explicit FadeTransition(
            std::shared_ptr<AnimationController> ctrl,
            Tween<float> op,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), opacity(op), child(std::move(c))
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
