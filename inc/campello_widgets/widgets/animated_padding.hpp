#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that animates the padding of its child between changes.
     *
     * When the `padding` property changes across widget updates the state
     * interpolates from the previous padding to the new one over
     * `duration_ms` using the supplied `curve`.
     *
     * Requires that a `TickerScheduler` has been created and registered (this
     * is handled automatically by `runApp()`).
     *
     * @code
     * auto w = std::make_shared<AnimatedPadding>();
     * w->padding     = expanded ? EdgeInsets::all(24.0f) : EdgeInsets::all(8.0f);
     * w->duration_ms = 250.0;
     * w->curve       = Curves::easeInOut;
     * w->child       = someChild;
     * @endcode
     */
    class AnimatedPadding : public StatefulWidget
    {
    public:
        EdgeInsets padding;
        WidgetRef  child;

        double                        duration_ms = 300.0;
        std::function<double(double)> curve       = Curves::easeInOut;

        AnimatedPadding() = default;
        explicit AnimatedPadding(EdgeInsets p, WidgetRef c = nullptr)
            : padding(std::move(p)), child(std::move(c))
        {
        }
        explicit AnimatedPadding(EdgeInsets p, double duration, WidgetRef c = nullptr)
            : padding(std::move(p)), child(std::move(c)), duration_ms(duration)
        {
        }

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
