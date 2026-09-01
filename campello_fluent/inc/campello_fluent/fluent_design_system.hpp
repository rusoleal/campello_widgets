#pragma once

#include <campello_widgets/ui/design_system.hpp>

namespace systems::leal::campello_widgets
{

/**
 * @brief Which Fluent material a surface is rendered with.
 *
 * Fluent 2 has three dominant background materials. They are not separate
 * themes; they are surface treatments within a single light/dark/high-contrast
 * theme, analogous to how CupertinoDesignSystem selects classic vs. liquid
 * glass.
 */
enum class FluentMaterial
{
    mica,    ///< Opaque-ish tinted base layer that samples the desktop/wallpaper.
    acrylic, ///< Translucent, noisy, blurred overlay used for transient surfaces.
    smoke,   ///< Darker, higher-contrast acrylic used for overlays/dialogs.
};

/**
 * @brief Microsoft Fluent 2 DesignSystem implementation.
 *
 * This is a sketch. It declares the full DesignSystem contract and provides
 * token presets for light, dark, and high-contrast modes, plus an optional
 * runtime accent color (Fluent derives its palette from the OS accent).
 *
 * Rendering the authentic Acrylic/Mica materials requires blur + noise +
 * luminosity/exclusion blend support in the backend; until those shaders
 * exist, surfaces fall back to solid fills from ColorScheme.
 */
class FluentDesignSystem : public DesignSystem
{
public:
    FluentDesignSystem();
    explicit FluentDesignSystem(DesignTokens tokens, Color accent = Color::fromARGB(0xFF0078D4));

    /** @brief Light-mode preset with the default Windows blue accent. */
    static FluentDesignSystem light(Color accent = Color::fromARGB(0xFF0078D4));

    /** @brief Dark-mode preset with the default Windows blue accent. */
    static FluentDesignSystem dark(Color accent = Color::fromARGB(0xFF0078D4));

    /** @brief High-contrast preset (OS-agnostic black/white/accent). */
    static FluentDesignSystem highContrast(Color accent = Color::fromARGB(0xFF0078D4));

    const DesignTokens& tokens() const override { return tokens_; }

    /** @brief Fluent accent color used to tint interactive controls. */
    Color accentColor() const noexcept { return accent_; }

    /** @brief Which background material this instance prefers for surfaces. */
    FluentMaterial preferredMaterial() const noexcept { return material_; }

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
    Color        accent_;
    FluentMaterial material_ = FluentMaterial::mica;

public:
    /** @brief Helper to apply an opacity multiplier to a color. */
    static Color withOpacity(Color c, float opacity);
};

} // namespace systems::leal::campello_widgets
