#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <functional>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive bottom navigation bar that delegates its visual
     *        appearance to the active DesignSystem.
     */
    class NavigationBar : public StatelessWidget
    {
    public:
        struct Item
        {
            WidgetRef   icon;
            std::string label;
        };

        std::vector<Item>        items;
        int                      selected_index = 0;
        std::function<void(int)> on_tap;

        NavigationBar() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
