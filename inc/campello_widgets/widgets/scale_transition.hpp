#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/alignment.hpp>

#include <functional>
#include <memory>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Explicitly animates the scale of a child using an external
     * AnimationController.
     *
     * The child is scaled uniformly around `alignment` (default: center).
     * A scale of 0 collapses to a point; 1 is the original size.
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(300.0);
     * ctrl->forward();
     *
     * auto w = std::make_shared<ScaleTransition>();
     * w->controller = ctrl;
     * w->scale      = {0.0f, 1.0f};  // collapses → full size
     * w->child      = someWidget;
     * @endcode
     */
    class ScaleTransition : public StatefulWidget
    {
    public:
        std::shared_ptr<AnimationController> controller;
        std::function<double(double)>        curve     = Curves::linear;
        Tween<float>                         scale     = {0.0f, 1.0f};
        Alignment                            alignment = Alignment::center();
        WidgetRef                            child;

        ScaleTransition() = default;
        explicit ScaleTransition(
            std::shared_ptr<AnimationController> ctrl,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), child(std::move(c))
        {
            CW_ASSERT_MSG(ctrl != nullptr, "ScaleTransition.controller must be set");
}
        explicit ScaleTransition(
            std::shared_ptr<AnimationController> ctrl,
            Tween<float> scl,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), scale(scl), child(std::move(c))
        {
            CW_ASSERT_MSG(ctrl != nullptr, "ScaleTransition.controller must be set");
}

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(controller != nullptr, "ScaleTransition.controller must be set");

        }

    };

} // namespace systems::leal::campello_widgets
