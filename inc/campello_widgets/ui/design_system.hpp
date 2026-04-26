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
    };

} // namespace systems::leal::campello_widgets
