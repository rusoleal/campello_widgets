#pragma once

#include <campello_widgets/ui/design_system.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Which visual material CupertinoDesignSystem renders its
     *        surfaces with.
     *
     * A style dimension on the *same* class/library rather than a separate
     * `campello_glass` library or a distinct widget set — Liquid Glass is
     * Apple's own evolution of Cupertino (same navigation idioms, same
     * component catalog, same iOS-specific behaviors), not a different
     * design philosophy the way Material vs. Cupertino are. See TODO.md's
     * Liquid Glass entry for the full rationale.
     */
    enum class CupertinoMaterial
    {
        classic,     ///< Flat/opaque HIG surfaces (pre-iOS/macOS 26).
        liquidGlass, ///< Translucent, refracted Liquid Glass surfaces (iOS/macOS 26+).
    };

    /**
     * @brief Apple HIG / Cupertino DesignSystem implementation.
     *
     * iOS system colors (systemBlue/systemGreen/systemRed/systemGray
     * scale), Dynamic Type sizes mapped onto the 15-role TypographyScale,
     * an iOS-appropriate shape scale (6/8/10/14/20pt — deliberately not
     * MD3's stadium-heavy scale), and much softer/flatter shadows than
     * Material (iOS avoids elevation-driven drop shadows almost entirely).
     *
     * Distinguishing HIG choices, versus campello_material's MD3 style:
     * filled buttons use a 14pt corner (not a stadium/pill), UISwitch's
     * real default is a *green* active track with an always-white thumb,
     * dialogs are narrow (270pt, matching UIAlertController) with centered
     * text and divided action buttons instead of a right-aligned row, and
     * NavigationBar's selected item gets no indicator pill at all — only a
     * tint color change, unlike MD3's tonal pill. There is no HIG
     * equivalent of a Floating Action Button; buildPrimaryActionButton
     * falls back to a plain circular button.
     *
     * `material()` selects between `classic` (the above) and
     * `liquidGlass` — currently wired into `buildCard()` (elevated
     * priority only) and `buildPrimaryActionButton()`, both fully-rounded
     * floating surfaces that fit the Liquid Glass shader's uniform
     * corner-radius shape model. Widgets that are flush with a screen edge
     * (NavigationBar/AppBar/BottomSheet — asymmetric rounding) or that use
     * the dedicated `Dialog` widget (which paints its own background,
     * with no backdrop-filter hook yet) aren't glass-able yet — see
     * TODO.md for the follow-up scope.
     */
    class CupertinoDesignSystem : public DesignSystem
    {
    public:
        CupertinoDesignSystem();
        explicit CupertinoDesignSystem(DesignTokens tokens, CupertinoMaterial material = CupertinoMaterial::classic);

        /** @brief Light-mode preset. */
        static CupertinoDesignSystem light();

        /** @brief Dark-mode preset. */
        static CupertinoDesignSystem dark();

        /** @brief Liquid Glass preset (iOS/macOS 26+ material), light or dark toned. */
        static CupertinoDesignSystem liquidGlass(bool dark = false);

        const DesignTokens& tokens() const override { return tokens_; }

        /** @brief Which material this instance renders surfaces with. */
        CupertinoMaterial material() const noexcept { return material_; }

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
        DesignTokens      tokens_;
        CupertinoMaterial material_ = CupertinoMaterial::classic;

        static Color withOpacity(Color c, float opacity);
    };

} // namespace systems::leal::campello_widgets
