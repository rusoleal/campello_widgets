#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that controls how a child of a Flex (Row/Column) flexes.
     *
     * When a Flex lays out its children it checks whether each child is a
     * Flexible. If so, the `flex` factor determines what fraction of the
     * remaining main-axis space the child receives.
     *
     * `Flexible::build()` is transparent — it simply returns its child, so the
     * child widget appears as a normal descendant in the element tree.
     */
    class Flexible : public StatelessWidget
    {
    public:
        int       flex  = 1;
        WidgetRef child;

        Flexible() = default;
        explicit Flexible(WidgetRef c) { child = std::move(c); }
        explicit Flexible(int f, WidgetRef c)
        {
            flex  = f;
            child = std::move(c);
        }

        WidgetRef build(BuildContext&) const override { return child; }

        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override
        {
            properties.add(std::make_unique<IntProperty>("flex", flex));
        }
    };

} // namespace systems::leal::campello_widgets
