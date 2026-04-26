#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive app bar that delegates its visual appearance to the
     *        active DesignSystem.
     */
    class AppBar : public StatelessWidget
    {
    public:
        WidgetRef              title;
        WidgetRef              leading;
        std::vector<WidgetRef> actions;
        bool                   center_title = false;

        AppBar() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
