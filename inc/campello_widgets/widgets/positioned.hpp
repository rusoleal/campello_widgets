#pragma once

#include <optional>
#include <campello_widgets/widgets/stateless_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that controls where a child of a Stack is positioned.
     *
     * Supply any combination of `left`, `top`, `right`, `bottom`, `width`,
     * and `height`. Unset edges/dimensions are unconstrained. At least one
     * horizontal and one vertical constraint must be provided for the child
     * to be positioned meaningfully.
     *
     * `Positioned::build()` is transparent — it returns its child unchanged.
     */
    class Positioned : public StatelessWidget
    {
    public:
        std::optional<float> left;
        std::optional<float> top;
        std::optional<float> right;
        std::optional<float> bottom;
        std::optional<float> width;
        std::optional<float> height;
        WidgetRef             child;

        WidgetRef build(BuildContext&) const override { return child; }
    };

} // namespace systems::leal::campello_widgets
