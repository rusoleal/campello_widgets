#pragma once

#include <optional>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A convenience widget that combines sizing, padding, color, and alignment.
     *
     * Container composes `SizedBox`, `Padding`, `ColoredBox`, and `Align` as
     * needed based on the properties that are set. If no properties are set it
     * is equivalent to a transparent box that fills available space.
     */
    class Container : public StatelessWidget
    {
    public:
        WidgetRef            child;
        std::optional<float> width;
        std::optional<float> height;
        std::optional<Color> color;
        std::optional<EdgeInsets> padding;
        std::optional<Alignment>  alignment;

        Container() = default;
        explicit Container(WidgetRef c) : child(std::move(c)) {}

        WidgetRef build(BuildContext& context) const override;
    };

} // namespace systems::leal::campello_widgets
