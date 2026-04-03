#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/offset.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Explicitly animates the position of a child using fractional offsets.
     *
     * The `offset` tween is expressed as a fraction of the child's own size:
     *  - `{-1, 0}` = one full width to the left (slide in from the left)
     *  - `{ 1, 0}` = one full width to the right
     *  - `{ 0, 1}` = one full height downward
     *
     * Default: slides in from the left (`begin = {-1,0}`, `end = {0,0}`).
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(350.0);
     * ctrl->forward();
     *
     * auto w = std::make_shared<SlideTransition>();
     * w->controller = ctrl;
     * w->offset     = {{-1.0f, 0.0f}, {0.0f, 0.0f}};  // slide in from left
     * w->curve      = Curves::easeOut;
     * w->child      = someWidget;
     * @endcode
     */
    class SlideTransition : public StatefulWidget
    {
    public:
        std::shared_ptr<AnimationController> controller;
        std::function<double(double)>        curve  = Curves::linear;
        Tween<Offset>                        offset = {{-1.0f, 0.0f}, {0.0f, 0.0f}};
        WidgetRef                            child;

        SlideTransition() = default;
        explicit SlideTransition(
            std::shared_ptr<AnimationController> ctrl,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), child(std::move(c))
        {}
        explicit SlideTransition(
            std::shared_ptr<AnimationController> ctrl,
            Tween<Offset> off,
            WidgetRef c = nullptr)
            : controller(std::move(ctrl)), offset(off), child(std::move(c))
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
