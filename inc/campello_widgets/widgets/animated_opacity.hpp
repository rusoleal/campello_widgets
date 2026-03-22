#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that animates the opacity of its child between changes.
     *
     * When the `opacity` property changes across widget updates the state
     * interpolates from the previous opacity to the new one over `duration_ms`
     * using the supplied `curve`.
     *
     * Requires that a `TickerScheduler` has been created and registered (this
     * is handled automatically by `runApp()`).
     *
     * @code
     * auto w = std::make_shared<AnimatedOpacity>();
     * w->opacity     = visible ? 1.0f : 0.0f;
     * w->duration_ms = 250.0;
     * w->curve       = Curves::easeInOut;
     * w->child       = someChild;
     * @endcode
     */
    class AnimatedOpacity : public StatefulWidget
    {
    public:
        float   opacity     = 1.0f;
        WidgetRef child;

        double                        duration_ms = 300.0;
        std::function<double(double)> curve       = Curves::easeInOut;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
