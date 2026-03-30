#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Animates between two child widgets when the child changes.
     *
     * When `child` is replaced the old child fades out while the new child
     * fades in. Both widgets are kept alive in a Stack during the transition.
     *
     * Child identity is determined by pointer equality. If you replace the
     * child widget with a new shared_ptr the transition will trigger.
     *
     * @code
     * auto w = std::make_shared<AnimatedSwitcher>();
     * w->duration_ms = 300.0;
     * w->curve       = Curves::easeInOut;
     * w->child       = showA ? widgetA : widgetB;
     * @endcode
     */
    class AnimatedSwitcher : public StatefulWidget
    {
    public:
        WidgetRef                     child;
        double                        duration_ms = 300.0;
        std::function<double(double)> curve       = Curves::easeInOut;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
