#pragma once

#include <campello_widgets/ui/design_tokens.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/widgets/widget.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    // -----------------------------------------------------------------------
    // Semantic intent configs — appearance-free
    // -----------------------------------------------------------------------

    enum class ButtonPriority
    {
        primary,
        secondary,
        tertiary,
        danger,
    };

    struct ButtonConfig
    {
        WidgetRef label;
        std::function<void()> on_pressed;
        bool enabled = true;
        ButtonPriority priority = ButtonPriority::primary;
        WidgetRef leading_icon;
        WidgetRef trailing_icon;
    };

    struct SwitchConfig
    {
        bool value = false;
        std::function<void(bool)> on_changed;
        bool enabled = true;
    };

    struct CheckboxConfig
    {
        bool value = false;
        std::function<void(bool)> on_changed;
        bool enabled = true;
        bool tristate = false;
    };

    struct RadioConfig
    {
        bool selected = false;
        std::function<void()> on_selected;
        bool enabled = true;
    };

    struct SliderConfig
    {
        float value = 0.0f;
        float min = 0.0f;
        float max = 1.0f;
        std::function<void(float)> on_changed;
        bool enabled = true;
    };

    struct TextFieldConfig
    {
        std::string placeholder;
        std::string value;
        std::function<void(std::string)> on_changed;
        std::function<void(std::string)> on_submitted;
        bool enabled = true;
        bool obscure_text = false;
        int max_lines = 1;
    };

    enum class CardPriority
    {
        elevated,
        filled,
        outlined,
    };

    struct CardConfig
    {
        WidgetRef child;
        CardPriority priority = CardPriority::elevated;
        EdgeInsets padding = EdgeInsets::all(16.0f);
    };

    enum class ProgressType
    {
        circular,
        linear,
    };

    struct ProgressConfig
    {
        ProgressType type = ProgressType::circular;
        std::optional<float> value; ///< null = indeterminate
    };

    struct TooltipConfig
    {
        std::string message;
        WidgetRef child;
    };

    struct ListTileConfig
    {
        WidgetRef leading;
        WidgetRef title;
        WidgetRef subtitle;
        WidgetRef trailing;
        std::function<void()> on_tap;
        std::function<void()> on_long_press;
        bool enabled = true;
    };

    struct DividerConfig
    {
        float indent = 0.0f;
        float end_indent = 0.0f;
    };

    struct AppBarConfig
    {
        WidgetRef title;
        WidgetRef leading;
        std::vector<WidgetRef> actions;
        bool center_title = false;
    };

    struct NavigationBarConfig
    {
        struct Item
        {
            WidgetRef icon;
            std::string label;
        };
        std::vector<Item> items;
        int selected_index = 0;
        std::function<void(int)> on_tap;
    };

    struct DialogConfig
    {
        WidgetRef title;
        WidgetRef content;
        std::vector<WidgetRef> actions;
    };

    struct SnackBarConfig
    {
        std::string message;
        std::optional<std::string> action_label;
        std::function<void()> on_action;
        double duration_ms = 4000.0;
    };

    struct PopupMenuConfig
    {
        struct Item
        {
            std::string label;
            std::function<void()> on_selected;
        };
        std::vector<Item> items;
        std::function<void(size_t)> on_selected;
        WidgetRef child; ///< Trigger widget
    };

    struct DropdownConfig
    {
        struct Item
        {
            std::string label;
            std::string value;
        };
        std::vector<Item> items;
        std::optional<std::string> selected_value;
        std::function<void(std::string)> on_changed;
        std::string hint;
    };

    struct PrimaryActionConfig
    {
        std::function<void()> on_pressed;
        WidgetRef icon;
        WidgetRef label;
        bool enabled = true;
    };

    struct TabBarConfig
    {
        struct Tab
        {
            std::string label;
            WidgetRef icon;
        };
        std::vector<Tab> tabs;
        int selected_index = 0;
        std::function<void(int)> on_tap;
    };

    enum class ChipPriority
    {
        assist,
        filter,
        input,
        suggestion,
    };

    struct ChipConfig
    {
        WidgetRef label;
        WidgetRef leading_icon;
        bool selected = false;
        bool enabled = true;
        ChipPriority priority = ChipPriority::assist;
        std::function<void()> on_selected;
        std::function<void()> on_deleted; ///< present -> shows a trailing delete affordance
    };

    struct SegmentedConfig
    {
        struct Segment
        {
            WidgetRef label;
            WidgetRef icon;
        };
        std::vector<Segment> segments;
        int selected_index = 0;
        std::function<void(int)> on_changed;
        bool enabled = true;
    };

    struct BottomSheetConfig
    {
        WidgetRef child;
        bool show_drag_handle = true;
    };

    struct BadgeConfig
    {
        WidgetRef child;
        std::optional<std::string> label; ///< nullopt -> small dot badge
    };

    struct IconButtonConfig
    {
        WidgetRef icon;
        std::function<void()> on_pressed;
        bool enabled = true;
        bool selected = false; ///< toggled/filled state, e.g. MD3 icon-button toggle
    };

    struct StepperConfig
    {
        int value = 0;
        int min = 0;
        int max = 100;
        int step = 1;
        std::function<void(int)> on_changed;
        bool enabled = true;
    };

    struct RatingConfig
    {
        int value = 0;
        int max = 5;
        std::function<void(int)> on_changed; ///< null -> read-only display
        bool enabled = true;
    };

    struct ActionSheetConfig
    {
        struct Action
        {
            std::string label;
            std::function<void()> on_selected;
            bool destructive = false;
        };
        WidgetRef title; ///< optional
        std::vector<Action> actions;
        std::function<void()> on_cancel; ///< present -> shows a Cancel action
    };

    struct SearchFieldConfig
    {
        std::string placeholder = "Search";
        std::string value;
        std::function<void(std::string)> on_changed;
        std::function<void()> on_clear; ///< shown as a trailing affordance when value is non-empty
        bool enabled = true;
    };

    /**
     * @brief A tappable field that displays a formatted date and opens a
     * picker on tap.
     *
     * Like Dialog/BottomSheet/PopupMenuButton, this builds visual chrome
     * only — it does not own calendar layout or picker presentation, which
     * stays the caller's responsibility (e.g. via an OverlayEntry).
     */
    struct DatePickerConfig
    {
        std::string label; ///< formatted date text, e.g. "Aug 14, 2026"
        std::function<void()> on_tap;
        bool enabled = true;
    };

    /** @brief Same trigger-field model as DatePickerConfig, for time values. */
    struct TimePickerConfig
    {
        std::string label; ///< formatted time text, e.g. "10:30 AM"
        std::function<void()> on_tap;
        bool enabled = true;
    };

    struct ExpansionTileConfig
    {
        WidgetRef title;
        WidgetRef subtitle;
        WidgetRef leading;
        WidgetRef children_content; ///< shown when expanded; caller composes (e.g. a Column)
        bool expanded = false;
        std::function<void(bool)> on_expansion_changed;
        bool enabled = true;
    };

    struct ToggleButtonsConfig
    {
        struct Item
        {
            WidgetRef label;
            WidgetRef icon;
            bool selected = false;
        };
        std::vector<Item> items;
        std::function<void(int)> on_pressed; ///< index tapped; caller updates items[i].selected before the next build
        bool enabled = true;
    };

    struct BannerConfig
    {
        WidgetRef content;
        WidgetRef leading;
        std::vector<WidgetRef> actions;
    };

    struct NavigationRailConfig
    {
        struct Item
        {
            WidgetRef icon;
            std::string label;
        };
        std::vector<Item> items;
        int selected_index = 0;
        std::function<void(int)> on_tap;
        bool extended = false; ///< show labels beside icons, vs. the default icon-only compact rail
    };

    /**
     * @brief A basic, read-only data table: header + rows.
     *
     * Deliberately scoped down — no sorting, pagination, or per-cell
     * editing — matching how DatePickerConfig/TimePickerConfig scope down
     * to trigger fields rather than full calendar/wheel widgets.
     */
    struct DataTableConfig
    {
        std::vector<std::string> columns;
        std::vector<std::vector<WidgetRef>> rows; ///< each row's size must match columns.size()
    };

    // -----------------------------------------------------------------------
    // Typography roles
    // -----------------------------------------------------------------------

    /**
     * @brief Semantic text roles for the typography scale.
     *
     * Each role maps to a specific style in the active design system's
     * TypographyScale. Use Theme::textStyleOf(context, role) to obtain
     * the concrete TextStyle for the current theme.
     */
    enum class TextRole
    {
        display_large,
        display_medium,
        display_small,
        headline_large,
        headline_medium,
        headline_small,
        title_large,
        title_medium,
        title_small,
        body_large,
        body_medium,
        body_small,
        label_large,
        label_medium,
        label_small,
    };

    /**
     * @brief Returns the TextStyle from a TypographyScale for the given role.
     */
    inline const TextStyle& textStyleForRole(const TypographyScale& scale, TextRole role)
    {
        switch (role) {
            case TextRole::display_large:  return scale.display_large;
            case TextRole::display_medium: return scale.display_medium;
            case TextRole::display_small:  return scale.display_small;
            case TextRole::headline_large: return scale.headline_large;
            case TextRole::headline_medium:return scale.headline_medium;
            case TextRole::headline_small: return scale.headline_small;
            case TextRole::title_large:    return scale.title_large;
            case TextRole::title_medium:   return scale.title_medium;
            case TextRole::title_small:    return scale.title_small;
            case TextRole::body_large:     return scale.body_large;
            case TextRole::body_medium:    return scale.body_medium;
            case TextRole::body_small:     return scale.body_small;
            case TextRole::label_large:    return scale.label_large;
            case TextRole::label_medium:   return scale.label_medium;
            case TextRole::label_small:    return scale.label_small;
        }
        return scale.body_medium; // fallback
    }

    // -----------------------------------------------------------------------
    // DesignSystem — abstract contract
    // -----------------------------------------------------------------------

    /**
     * @brief Abstract design system interface.
     *
     * A DesignSystem implementation decides how every elemental UI widget
     * looks and behaves. It receives semantic config structs (intent) and
     * returns concrete widget trees (appearance).
     *
     * The base framework provides adaptive wrappers (Button, Switch, etc.)
     * that call Theme::of(ctx).buildXxx(config), so users don't interact
     * with the DesignSystem directly.
     */
    class DesignSystem
    {
    public:
        virtual ~DesignSystem() = default;

        /** @brief The raw design tokens used by this implementation. */
        virtual const DesignTokens& tokens() const = 0;

        // Component builders
        virtual WidgetRef buildButton(const ButtonConfig&) const = 0;
        virtual WidgetRef buildSwitch(const SwitchConfig&) const = 0;
        virtual WidgetRef buildCheckbox(const CheckboxConfig&) const = 0;
        virtual WidgetRef buildRadio(const RadioConfig&) const = 0;
        virtual WidgetRef buildSlider(const SliderConfig&) const = 0;
        virtual WidgetRef buildTextField(const TextFieldConfig&) const = 0;
        virtual WidgetRef buildCard(const CardConfig&) const = 0;
        virtual WidgetRef buildProgressIndicator(const ProgressConfig&) const = 0;
        virtual WidgetRef buildTooltip(const TooltipConfig&) const = 0;
        virtual WidgetRef buildListTile(const ListTileConfig&) const = 0;
        virtual WidgetRef buildDivider(const DividerConfig&) const = 0;
        virtual WidgetRef buildAppBar(const AppBarConfig&) const = 0;
        virtual WidgetRef buildNavigationBar(const NavigationBarConfig&) const = 0;
        virtual WidgetRef buildDialog(const DialogConfig&) const = 0;
        virtual WidgetRef buildSnackBar(const SnackBarConfig&) const = 0;
        virtual WidgetRef buildPopupMenuButton(const PopupMenuConfig&) const = 0;
        virtual WidgetRef buildDropdownButton(const DropdownConfig&) const = 0;
        virtual WidgetRef buildPrimaryActionButton(const PrimaryActionConfig&) const = 0;
        virtual WidgetRef buildTabBar(const TabBarConfig&) const = 0;
        virtual WidgetRef buildChip(const ChipConfig&) const = 0;
        virtual WidgetRef buildSegmentedButton(const SegmentedConfig&) const = 0;
        virtual WidgetRef buildBottomSheet(const BottomSheetConfig&) const = 0;
        virtual WidgetRef buildBadge(const BadgeConfig&) const = 0;
        virtual WidgetRef buildIconButton(const IconButtonConfig&) const = 0;
        virtual WidgetRef buildStepper(const StepperConfig&) const = 0;
        virtual WidgetRef buildRatingIndicator(const RatingConfig&) const = 0;
        virtual WidgetRef buildActionSheet(const ActionSheetConfig&) const = 0;
        virtual WidgetRef buildSearchField(const SearchFieldConfig&) const = 0;
        virtual WidgetRef buildDatePicker(const DatePickerConfig&) const = 0;
        virtual WidgetRef buildTimePicker(const TimePickerConfig&) const = 0;
        virtual WidgetRef buildExpansionTile(const ExpansionTileConfig&) const = 0;
        virtual WidgetRef buildToggleButtons(const ToggleButtonsConfig&) const = 0;
        virtual WidgetRef buildBanner(const BannerConfig&) const = 0;
        virtual WidgetRef buildNavigationRail(const NavigationRailConfig&) const = 0;
        virtual WidgetRef buildDataTable(const DataTableConfig&) const = 0;
    };

} // namespace systems::leal::campello_widgets
