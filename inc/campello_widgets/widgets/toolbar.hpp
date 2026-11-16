#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive toolbar (MD3 Expressive floating toolbar, Fluent
     *        CommandBar, Cupertino capsule toolbar) whose look comes from the
     *        active DesignSystem. Items in groups, each an icon and/or a
     *        label with a tooltip and a selected state (a tool palette's
     *        active tool). Put it in a Stack over a viewer to float it.
     *
     * @code
     * auto bar = std::make_shared<Toolbar>();
     * bar->groups = {{{.icon = select, .tooltip = "Select", .on_pressed = pick, .selected = true}},
     *                {{.label = "Wall", .tooltip = "Wall (W)", .on_pressed = wall}}};
     * @endcode
     */
    class Toolbar : public StatelessWidget
    {
    public:
        std::vector<std::vector<ToolbarConfig::Item>> groups;
        ToolbarColor color = ToolbarColor::standard;
        Axis direction = Axis::horizontal;
        bool floating = true;
        bool elevated = false;
        bool overflow = true;
        bool dividers = false;  ///< see ToolbarConfig::dividers

        Toolbar() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
