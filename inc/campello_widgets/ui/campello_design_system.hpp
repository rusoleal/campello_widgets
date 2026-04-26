#pragma once

#include <campello_widgets/ui/design_system.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief The canonical Campello design system implementation.
     *
     * A warm, approachable visual style distinct from both Material Design and
     * Cupertino. Uses a teal primary palette, generous rounding (8 px), and
     * softer shadows.
     */
    class CampelloDesignSystem : public DesignSystem
    {
    public:
        CampelloDesignSystem();
        explicit CampelloDesignSystem(DesignTokens tokens);

        /** @brief Light-mode preset. */
        static CampelloDesignSystem light();

        /** @brief Dark-mode preset. */
        static CampelloDesignSystem dark();

        const DesignTokens& tokens() const override { return tokens_; }

        // Component builders
        WidgetRef buildButton(const ButtonConfig&) const override;
        WidgetRef buildSwitch(const SwitchConfig&) const override;
        WidgetRef buildCheckbox(const CheckboxConfig&) const override;
        WidgetRef buildRadio(const RadioConfig&) const override;
        WidgetRef buildSlider(const SliderConfig&) const override;
        WidgetRef buildTextField(const TextFieldConfig&) const override;
        WidgetRef buildCard(const CardConfig&) const override;
        WidgetRef buildProgressIndicator(const ProgressConfig&) const override;
        WidgetRef buildTooltip(const TooltipConfig&) const override;
        WidgetRef buildListTile(const ListTileConfig&) const override;
        WidgetRef buildDivider(const DividerConfig&) const override;
        WidgetRef buildAppBar(const AppBarConfig&) const override;
        WidgetRef buildNavigationBar(const NavigationBarConfig&) const override;
        WidgetRef buildDialog(const DialogConfig&) const override;
        WidgetRef buildSnackBar(const SnackBarConfig&) const override;
        WidgetRef buildPopupMenuButton(const PopupMenuConfig&) const override;
        WidgetRef buildDropdownButton(const DropdownConfig&) const override;
        WidgetRef buildPrimaryActionButton(const PrimaryActionConfig&) const override;
        WidgetRef buildTabBar(const TabBarConfig&) const override;

    private:
        DesignTokens tokens_;

        // Helper to compute a disabled color
        static Color withOpacity(Color c, float opacity);
    };

} // namespace systems::leal::campello_widgets
