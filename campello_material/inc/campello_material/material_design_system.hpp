#pragma once

#include <campello_widgets/ui/design_system.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Material Design 3 DesignSystem implementation.
     *
     * Baseline MD3 tonal palette (seed color #6750A4), MD3 type scale,
     * MD3 shape scale (4/8/12/16/28dp), and MD3 elevation levels
     * (0/1/3/6/8/12dp — already the DesignTokens::ElevationTokens default).
     *
     * Distinguishing MD3 shape choices, versus campello_ui's rounded-rect
     * style: filled buttons are fully rounded (stadium/pill shape), the FAB
     * is a 16dp-radius rounded square (MD3 changed this from the circular
     * FAB of Material 2), and the selected item in a NavigationBar gets a
     * pill-shaped tonal indicator behind its icon.
     */
    class MaterialDesignSystem : public DesignSystem
    {
    public:
        MaterialDesignSystem();
        explicit MaterialDesignSystem(DesignTokens tokens);

        /** @brief Light-mode preset (MD3 baseline light scheme). */
        static MaterialDesignSystem light();

        /** @brief Dark-mode preset (MD3 baseline dark scheme). */
        static MaterialDesignSystem dark();

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
        WidgetRef buildChip(const ChipConfig&) const override;
        WidgetRef buildSegmentedButton(const SegmentedConfig&) const override;
        WidgetRef buildBottomSheet(const BottomSheetConfig&) const override;
        WidgetRef buildBadge(const BadgeConfig&) const override;
        WidgetRef buildIconButton(const IconButtonConfig&) const override;
        WidgetRef buildStepper(const StepperConfig&) const override;
        WidgetRef buildRatingIndicator(const RatingConfig&) const override;
        WidgetRef buildActionSheet(const ActionSheetConfig&) const override;
        WidgetRef buildSearchField(const SearchFieldConfig&) const override;
        WidgetRef buildDatePicker(const DatePickerConfig&) const override;
        WidgetRef buildTimePicker(const TimePickerConfig&) const override;
        WidgetRef buildExpansionTile(const ExpansionTileConfig&) const override;
        WidgetRef buildToggleButtons(const ToggleButtonsConfig&) const override;
        WidgetRef buildBanner(const BannerConfig&) const override;
        WidgetRef buildNavigationRail(const NavigationRailConfig&) const override;
        WidgetRef buildDataTable(const DataTableConfig&) const override;

    private:
        DesignTokens tokens_;

        static Color withOpacity(Color c, float opacity);
    };

} // namespace systems::leal::campello_widgets
