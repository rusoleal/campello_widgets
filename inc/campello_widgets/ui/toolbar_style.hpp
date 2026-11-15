#pragma once

#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/design_system.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/text_style.hpp>

#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief The look of a toolbar, which every DesignSystem's
     * buildToolbar() supplies to the shared layout (buildToolbarWithStyle):
     * the container, the items (pill-shaped buttons with a hover overlay,
     * filled when selected), the gaps and the dividers between groups.
     */
    struct ToolbarStyle
    {
        Color container;                  ///< floating container fill
        Color docked_container;           ///< fill when not floating
        Color foreground;                 ///< icons and labels
        Color selected_container;         ///< a selected item's fill
        Color selected_foreground;
        /// State layers over an item, in its content colour (MD3
        /// StateTokens: hover 0.08, pressed 0.10).
        float hover_opacity = 0.08f;
        float pressed_opacity = 0.10f;
        Color divider;                    ///< between groups (alpha 0: none)
        float radius = 9999.0f;           ///< container corners (9999: stadium)
        float item_radius = 9999.0f;
        EdgeInsets padding = EdgeInsets::all(8.0f);
        float item_extent = 48.0f;        ///< items' touch target: height (and width for icon-only items)
        float indicator_extent = 40.0f;   ///< the selected fill and state layers, centred in the target
        float item_padding = 12.0f;       ///< along the axis, around labels
        float gap = 4.0f;                 ///< between items
        float group_gap = 8.0f;           ///< between groups (each side of a divider)
        float icon_size = 24.0f;
        TextStyle label_style;
        std::vector<BoxShadow> shadow;    ///< floating and elevated only
        float disabled_opacity = 0.38f;
    };

    /** @brief The toolbar laid out with `style` (row or column of groups). */
    WidgetRef buildToolbarWithStyle(const ToolbarConfig& config, const ToolbarStyle& style, bool pointer_cursor);

} // namespace systems::leal::campello_widgets
