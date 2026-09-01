#pragma once

#include <campello_widgets/ui/design_system.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A minimal, dependency-free DesignSystem with no visual opinion.
     *
     * Flat neutral grays, sharp corners, no shadows. This is core's
     * no-`Theme`-found fallback so `Theme::of()` never returns a null-ish
     * state — mirroring how Flutter's `WidgetsApp` renders unstyled without
     * `MaterialApp`/`CupertinoApp` on top. Real applications should wrap
     * their root in a `Theme` using `campello_ui`, `campello_material`, or
     * `campello_cupertino` instead of relying on this.
     */
    class NullDesignSystem : public DesignSystem
    {
    public:
        NullDesignSystem();

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
        WidgetRef buildRefreshIndicator(const RefreshIndicatorConfig&) const override;
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
    };

} // namespace systems::leal::campello_widgets
