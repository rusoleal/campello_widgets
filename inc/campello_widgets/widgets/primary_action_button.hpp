#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive primary action button (FAB) that delegates its visual
     *        appearance to the active DesignSystem.
     */
    class PrimaryActionButton : public StatelessWidget
    {
    public:
        std::function<void()> on_pressed;
        WidgetRef             icon;
        WidgetRef             label;
        bool                  enabled = true;

        PrimaryActionButton() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
