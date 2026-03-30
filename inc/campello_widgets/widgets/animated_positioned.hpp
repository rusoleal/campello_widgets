#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Implicitly animates the position of a Stack child whenever
     * any positioned property changes across widget updates.
     *
     * Use inside a `Stack`. All geometry properties are optional; unset
     * properties behave the same as in `Positioned` (unconstrained).
     *
     * @code
     * // In Stack's children list:
     * auto w = std::make_shared<AnimatedPositioned>();
     * w->left        = pressed ? 50.0f : 0.0f;
     * w->top         = 20.0f;
     * w->duration_ms = 250.0;
     * w->child       = someChild;
     * @endcode
     */
    class AnimatedPositioned : public StatefulWidget
    {
    public:
        std::optional<float>          left;
        std::optional<float>          top;
        std::optional<float>          right;
        std::optional<float>          bottom;
        std::optional<float>          width;
        std::optional<float>          height;

        double                        duration_ms = 300.0;
        std::function<double(double)> curve       = Curves::easeInOut;
        WidgetRef                     child;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
