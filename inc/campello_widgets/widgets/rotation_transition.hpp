#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/alignment.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Explicitly animates the rotation of a child using an external
     * AnimationController.
     *
     * The `turns` tween is expressed in full turns (1 turn = 360°).
     * Default: 0 → 1 turn (one full clockwise rotation).
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(600.0);
     * ctrl->forward();
     *
     * auto w = std::make_shared<RotationTransition>();
     * w->controller = ctrl;
     * w->turns      = {0.0f, 0.25f};   // 0 → 90° clockwise
     * w->curve      = Curves::easeInOut;
     * w->child      = someWidget;
     * @endcode
     */
    class RotationTransition : public StatefulWidget
    {
    public:
        std::shared_ptr<AnimationController> controller;
        std::function<double(double)>        curve     = Curves::linear;
        Tween<float>                         turns     = {0.0f, 1.0f};
        Alignment                            alignment = Alignment::center();
        WidgetRef                            child;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
