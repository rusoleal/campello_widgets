#include <campello_ui/campello_design_system.hpp>

// Widgets
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/dropdown_button.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/floating_action_button.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/linear_progress_indicator.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/radio.hpp>
#include <campello_widgets/widgets/radio_group.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/slider.hpp>
#include <campello_widgets/widgets/snack_bar.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/switch.hpp>
#include <campello_widgets/widgets/tab_bar.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/tooltip.hpp>
#include <campello_widgets/widgets/popup_menu_button.hpp>
#include <campello_widgets/widgets/dialog.hpp>

// UI primitives
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace systems::leal::campello_widgets
{

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    Color CampelloDesignSystem::withOpacity(Color c, float opacity)
    {
        return Color::fromRGBA(c.r, c.g, c.b, c.a * opacity);
    }

    // -----------------------------------------------------------------------
    // Token factories
    // -----------------------------------------------------------------------

    static TypographyScale makeLightTypography()
    {
        TypographyScale t;
        auto on_surface = Color::fromRGB(0.118f, 0.129f, 0.149f);

        t.display_large  = TextStyle{}.withFontSize(57.0f).bold().withColor(on_surface);
        t.display_medium = TextStyle{}.withFontSize(45.0f).bold().withColor(on_surface);
        t.display_small  = TextStyle{}.withFontSize(36.0f).bold().withColor(on_surface);
        t.headline_large = TextStyle{}.withFontSize(32.0f).bold().withColor(on_surface);
        t.headline_medium= TextStyle{}.withFontSize(28.0f).bold().withColor(on_surface);
        t.headline_small = TextStyle{}.withFontSize(24.0f).bold().withColor(on_surface);
        t.title_large    = TextStyle{}.withFontSize(22.0f).bold().withColor(on_surface);
        t.title_medium   = TextStyle{}.withFontSize(16.0f).bold().withColor(on_surface);
        t.title_small    = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
        t.body_large     = TextStyle{}.withFontSize(16.0f).withColor(on_surface);
        t.body_medium    = TextStyle{}.withFontSize(14.0f).withColor(on_surface);
        t.body_small     = TextStyle{}.withFontSize(12.0f).withColor(on_surface);
        t.label_large    = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
        t.label_medium   = TextStyle{}.withFontSize(12.0f).bold().withColor(on_surface);
        t.label_small    = TextStyle{}.withFontSize(11.0f).bold().withColor(on_surface);
        return t;
    }

    static TypographyScale makeDarkTypography()
    {
        TypographyScale t;
        auto on_surface = Color::fromRGB(0.910f, 0.886f, 0.859f);

        t.display_large  = TextStyle{}.withFontSize(57.0f).bold().withColor(on_surface);
        t.display_medium = TextStyle{}.withFontSize(45.0f).bold().withColor(on_surface);
        t.display_small  = TextStyle{}.withFontSize(36.0f).bold().withColor(on_surface);
        t.headline_large = TextStyle{}.withFontSize(32.0f).bold().withColor(on_surface);
        t.headline_medium= TextStyle{}.withFontSize(28.0f).bold().withColor(on_surface);
        t.headline_small = TextStyle{}.withFontSize(24.0f).bold().withColor(on_surface);
        t.title_large    = TextStyle{}.withFontSize(22.0f).bold().withColor(on_surface);
        t.title_medium   = TextStyle{}.withFontSize(16.0f).bold().withColor(on_surface);
        t.title_small    = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
        t.body_large     = TextStyle{}.withFontSize(16.0f).withColor(on_surface);
        t.body_medium    = TextStyle{}.withFontSize(14.0f).withColor(on_surface);
        t.body_small     = TextStyle{}.withFontSize(12.0f).withColor(on_surface);
        t.label_large    = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
        t.label_medium   = TextStyle{}.withFontSize(12.0f).bold().withColor(on_surface);
        t.label_small    = TextStyle{}.withFontSize(11.0f).bold().withColor(on_surface);
        return t;
    }

    static DesignTokens makeLightTokens()
    {
        DesignTokens t;
        t.brightness = Brightness::light;

        // Campello warm teal primary
        t.colors.primary         = Color::fromRGB(0.051f, 0.545f, 0.553f);
        t.colors.on_primary      = Color::white();
        t.colors.primary_container    = Color::fromRGB(0.784f, 0.925f, 0.922f);
        t.colors.on_primary_container = Color::fromRGB(0.0f, 0.184f, 0.184f);
        t.colors.secondary       = Color::fromRGB(0.910f, 0.608f, 0.149f);
        t.colors.on_secondary    = Color::fromRGB(0.102f, 0.102f, 0.102f);
        t.colors.secondary_container    = Color::fromRGB(1.0f, 0.914f, 0.784f);
        t.colors.on_secondary_container = Color::fromRGB(0.302f, 0.196f, 0.0f);
        // Tertiary — a warm coral/terracotta, complementing teal + amber.
        t.colors.tertiary         = Color::fromRGB(0.878f, 0.447f, 0.310f);
        t.colors.on_tertiary      = Color::white();
        t.colors.tertiary_container    = Color::fromRGB(1.0f, 0.878f, 0.835f);
        t.colors.on_tertiary_container = Color::fromRGB(0.322f, 0.129f, 0.055f);
        t.colors.surface         = Color::fromRGB(0.980f, 0.973f, 0.961f);
        t.colors.on_surface      = Color::fromRGB(0.118f, 0.129f, 0.149f);
        t.colors.surface_variant = Color::fromRGB(0.941f, 0.929f, 0.910f);
        t.colors.on_surface_variant = Color::fromRGB(0.353f, 0.373f, 0.400f);
        t.colors.background      = Color::white();
        t.colors.on_background   = Color::fromRGB(0.118f, 0.129f, 0.149f);
        t.colors.error           = Color::fromRGB(0.910f, 0.365f, 0.365f);
        t.colors.on_error        = Color::white();
        t.colors.error_container    = Color::fromRGB(1.0f, 0.878f, 0.878f);
        t.colors.on_error_container = Color::fromRGB(0.322f, 0.055f, 0.055f);
        t.colors.success         = Color::fromRGB(0.180f, 0.686f, 0.420f);
        t.colors.on_success      = Color::white();
        t.colors.warning         = Color::fromRGB(0.941f, 0.627f, 0.188f);
        t.colors.on_warning      = Color::fromRGB(0.102f, 0.102f, 0.102f);
        t.colors.outline         = Color::fromRGB(0.831f, 0.812f, 0.780f);
        t.colors.outline_variant = Color::fromRGB(0.910f, 0.890f, 0.859f);
        t.colors.shadow          = Color::fromRGBA(0.0f, 0.0f, 0.0f, 1.0f);
        t.colors.scrim           = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.5f);
        t.colors.inverse_surface = Color::fromRGB(0.118f, 0.129f, 0.149f);
        t.colors.inverse_on_surface = Color::fromRGB(0.980f, 0.973f, 0.961f);
        t.colors.inverse_primary = Color::fromRGB(0.494f, 0.835f, 0.839f);

        t.elevation = ElevationTokens{};
        t.shape = ShapeTokens{};
        t.spacing = SpacingTokens{};
        t.typography = makeLightTypography();
        t.motion = MotionTokens{};
        return t;
    }

    static DesignTokens makeDarkTokens()
    {
        DesignTokens t;
        t.brightness = Brightness::dark;

        t.colors.primary         = Color::fromRGB(0.306f, 0.804f, 0.769f);
        t.colors.on_primary      = Color::fromRGB(0.0f, 0.216f, 0.208f);
        t.colors.primary_container    = Color::fromRGB(0.0f, 0.322f, 0.318f);
        t.colors.on_primary_container = Color::fromRGB(0.784f, 0.925f, 0.922f);
        t.colors.secondary       = Color::fromRGB(0.957f, 0.722f, 0.376f);
        t.colors.on_secondary    = Color::fromRGB(0.239f, 0.157f, 0.0f);
        t.colors.secondary_container    = Color::fromRGB(0.361f, 0.243f, 0.0f);
        t.colors.on_secondary_container = Color::fromRGB(1.0f, 0.914f, 0.784f);
        t.colors.tertiary         = Color::fromRGB(0.949f, 0.616f, 0.502f);
        t.colors.on_tertiary      = Color::fromRGB(0.322f, 0.129f, 0.055f);
        t.colors.tertiary_container    = Color::fromRGB(0.443f, 0.216f, 0.129f);
        t.colors.on_tertiary_container = Color::fromRGB(1.0f, 0.878f, 0.835f);
        t.colors.surface         = Color::fromRGB(0.118f, 0.129f, 0.149f);
        t.colors.on_surface      = Color::fromRGB(0.910f, 0.886f, 0.859f);
        t.colors.surface_variant = Color::fromRGB(0.165f, 0.184f, 0.212f);
        t.colors.on_surface_variant = Color::fromRGB(0.659f, 0.635f, 0.604f);
        t.colors.background      = Color::fromRGB(0.071f, 0.078f, 0.094f);
        t.colors.on_background   = Color::fromRGB(0.910f, 0.886f, 0.859f);
        t.colors.error           = Color::fromRGB(0.949f, 0.545f, 0.545f);
        t.colors.on_error        = Color::fromRGB(0.239f, 0.0f, 0.0f);
        t.colors.error_container    = Color::fromRGB(0.443f, 0.129f, 0.129f);
        t.colors.on_error_container = Color::fromRGB(1.0f, 0.878f, 0.878f);
        t.colors.success         = Color::fromRGB(0.361f, 0.839f, 0.600f);
        t.colors.on_success      = Color::fromRGB(0.0f, 0.239f, 0.122f);
        t.colors.warning         = Color::fromRGB(0.961f, 0.784f, 0.290f);
        t.colors.on_warning      = Color::fromRGB(0.239f, 0.157f, 0.0f);
        t.colors.outline         = Color::fromRGB(0.353f, 0.373f, 0.400f);
        t.colors.outline_variant = Color::fromRGB(0.227f, 0.247f, 0.275f);
        t.colors.shadow          = Color::fromRGBA(0.0f, 0.0f, 0.0f, 1.0f);
        t.colors.scrim           = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.6f);
        t.colors.inverse_surface = Color::fromRGB(0.980f, 0.973f, 0.961f);
        t.colors.inverse_on_surface = Color::fromRGB(0.118f, 0.129f, 0.149f);
        t.colors.inverse_primary = Color::fromRGB(0.051f, 0.545f, 0.553f);

        t.elevation = ElevationTokens{};
        t.shape = ShapeTokens{};
        t.spacing = SpacingTokens{};
        t.typography = makeDarkTypography();
        t.motion = MotionTokens{};
        return t;
    }

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    CampelloDesignSystem::CampelloDesignSystem()
        : tokens_(makeLightTokens())
    {}

    CampelloDesignSystem::CampelloDesignSystem(DesignTokens tokens)
        : tokens_(std::move(tokens))
    {}

    CampelloDesignSystem CampelloDesignSystem::light()
    {
        return CampelloDesignSystem(makeLightTokens());
    }

    CampelloDesignSystem CampelloDesignSystem::dark()
    {
        return CampelloDesignSystem(makeDarkTokens());
    }


    // -----------------------------------------------------------------------
    // Button
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildButton(const ButtonConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        Color bg, fg;

        switch (cfg.priority) {
            case ButtonPriority::secondary:
                bg = c.secondary;
                fg = c.on_secondary;
                break;
            case ButtonPriority::tertiary:
                bg = c.surface_variant;
                fg = c.on_surface;
                break;
            case ButtonPriority::danger:
                bg = c.error;
                fg = c.on_error;
                break;
            case ButtonPriority::primary:
            default:
                bg = c.primary;
                fg = c.on_primary;
                break;
        }

        if (!cfg.enabled) {
            bg = withOpacity(bg, 0.4f);
            fg = withOpacity(fg, 0.4f);
        }

        // Compose label + optional icons
        WidgetRef content = cfg.label;
        if (cfg.leading_icon || cfg.trailing_icon) {
            auto row = std::make_shared<Row>();
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            if (cfg.leading_icon)
                row->children.push_back(cfg.leading_icon);
            row->children.push_back(cfg.label);
            if (cfg.trailing_icon)
                row->children.push_back(cfg.trailing_icon);
            content = row;
        }

        // Padding + child
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(20.0f, 12.0f);
        padded->child   = content;

        // Decoration: rounded rect + background + optional elevation shadow
        BoxDecoration deco;
        deco.color         = bg;
        deco.border_radius = tokens_.shape.radius_md; // 8 px
        if (tokens_.elevation.level2 > 0.0f) {
            float el = tokens_.elevation.level2;
            deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                    Offset{0.0f, el * 0.5f},
                    el * 2.0f
                }
            };
        }

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        // Tap handler
        auto detector = std::make_shared<GestureDetector>();
        detector->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
        detector->child  = decorated;

        // Disabled state: reduce opacity
        if (!cfg.enabled || !cfg.on_pressed) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = detector;
            return faded;
        }

        return detector;
    }

    // -----------------------------------------------------------------------
    // Switch
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildSwitch(const SwitchConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sw = std::make_shared<Switch>();
        sw->value = cfg.value;
        sw->active_track_color   = withOpacity(c.primary, 0.5f);
        sw->inactive_track_color = withOpacity(c.on_surface, 0.26f);
        sw->active_thumb_color   = cfg.value ? c.primary : c.surface;
        sw->inactive_thumb_color = c.surface;

        if (cfg.enabled && cfg.on_changed) {
            sw->on_changed = cfg.on_changed;
        } else {
            sw->on_changed = nullptr;
            // Render at reduced opacity when disabled
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = sw;
            return faded;
        }
        return sw;
    }

    // -----------------------------------------------------------------------
    // Checkbox
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildCheckbox(const CheckboxConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto cb = std::make_shared<Checkbox>();
        cb->value = cfg.value;
        cb->active_color  = c.primary;
        cb->check_color   = c.on_primary;
        cb->border_color  = c.outline;
        cb->border_radius = tokens_.shape.radius_xs; // 2 px

        if (cfg.enabled && cfg.on_changed) {
            cb->on_changed = cfg.on_changed;
        } else {
            cb->on_changed = nullptr;
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = cb;
            return faded;
        }
        return cb;
    }

    // -----------------------------------------------------------------------
    // Radio
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildRadio(const RadioConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto rd = std::make_shared<Radio>(0); // value handled by RadioGroup
        rd->active_color   = c.primary;
        rd->inactive_color = c.outline;
        rd->size = 20.0f;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = rd;
            return faded;
        }
        return rd;
    }

    // -----------------------------------------------------------------------
    // Slider
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildSlider(const SliderConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sl = std::make_shared<Slider>();
        sl->value = cfg.value;
        sl->min   = cfg.min;
        sl->max   = cfg.max;
        sl->active_color   = c.primary;
        sl->inactive_color = c.outline;
        sl->track_height   = 4.0f;
        sl->thumb_radius   = 10.0f;

        if (cfg.enabled && cfg.on_changed) {
            sl->on_changed = cfg.on_changed;
        } else {
            sl->on_changed = nullptr;
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = sl;
            return faded;
        }
        return sl;
    }

    // -----------------------------------------------------------------------
    // TextField
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildTextField(const TextFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tf = std::make_shared<TextField>();
        tf->placeholder        = cfg.placeholder;
        tf->obscure_text       = cfg.obscure_text;
        tf->max_lines          = cfg.max_lines;
        tf->fill_color         = c.surface;
        tf->border_color       = c.outline;
        tf->focused_border_color = c.primary;
        tf->cursor_color       = c.primary;
        tf->selection_color    = withOpacity(c.primary, 0.3f);
        tf->placeholder_color  = c.on_surface_variant;
        tf->border_radius      = tokens_.shape.radius_md; // 8 px
        tf->border_width       = 1.0f;
        tf->min_height         = 44.0f;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = tf;
            return faded;
        }

        if (cfg.on_changed)
            tf->on_changed = [cb = cfg.on_changed](const std::string& v) { cb(v); };
        if (cfg.on_submitted)
            tf->on_submitted = [cb = cfg.on_submitted](const std::string& v) { cb(v); };

        return tf;
    }


    // -----------------------------------------------------------------------
    // Card
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildCard(const CardConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        BoxDecoration deco;
        deco.border_radius = tokens_.shape.radius_md; // 8 px

        switch (cfg.priority) {
            case CardPriority::filled:
                deco.color = c.surface_variant;
                break;
            case CardPriority::outlined:
                deco.color  = c.surface;
                deco.border = BoxBorder::all(c.outline, 1.0f);
                break;
            case CardPriority::elevated:
            default:
                deco.color = c.surface;
                float el = tokens_.elevation.level2;
                deco.box_shadow = {
                    BoxShadow{
                        Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.12f),
                        Offset{0.0f, el * 0.5f},
                        el * 2.0f
                    }
                };
                break;
        }

        auto inner = std::make_shared<Padding>();
        inner->padding = cfg.padding;
        inner->child   = cfg.child;

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = inner;

        return decorated;
    }

    // -----------------------------------------------------------------------
    // ProgressIndicator
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildProgressIndicator(const ProgressConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        if (cfg.type == ProgressType::circular) {
            auto pi = std::make_shared<CircularProgressIndicator>();
            if (cfg.value.has_value())
                pi->value = *cfg.value;
            pi->value_color      = c.primary;
            pi->background_color = withOpacity(c.primary, 0.15f);
            pi->stroke_width     = 3.0f;
            pi->size             = 36.0f;
            return pi;
        } else {
            auto pi = std::make_shared<LinearProgressIndicator>();
            if (cfg.value.has_value())
                pi->value = *cfg.value;
            pi->value_color      = c.primary;
            pi->background_color = withOpacity(c.primary, 0.15f);
            pi->min_height       = 4.0f;
            return pi;
        }
    }

    // -----------------------------------------------------------------------
    // Tooltip
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildTooltip(const TooltipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tt = std::make_shared<Tooltip>();
        tt->message             = cfg.message;
        tt->child               = cfg.child;
        tt->background_color    = c.inverse_surface;
        tt->text_color          = c.inverse_on_surface;
        tt->border_radius       = tokens_.shape.radius_sm; // 4 px
        tt->padding             = EdgeInsets::symmetric(10.0f, 6.0f);
        tt->font_size           = 12.0f;
        tt->display_duration_ms = 2500.0;
        return tt;
    }

    // -----------------------------------------------------------------------
    // ListTile
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildListTile(const ListTileConfig& cfg) const
    {
        // Title + optional subtitle column
        WidgetRef text_section;
        if (cfg.subtitle) {
            auto col  = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::start;
            col->children = {cfg.title, cfg.subtitle};
            text_section  = col;
        } else {
            text_section = cfg.title;
        }

        // Main row
        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(16.0f));
        }
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(text_section)));
        if (cfg.trailing) {
            row_children.push_back(SizedBox::from_width(16.0f));
            row_children.push_back(cfg.trailing);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        // Content padding
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
        padded->child   = row;

        // Minimum height
        const float min_h = cfg.subtitle ? 72.0f : 56.0f;
        const float inf   = std::numeric_limits<float>::infinity();
        auto constrained = std::make_shared<ConstrainedBox>();
        constrained->additional_constraints = BoxConstraints{0.0f, inf, min_h, inf};
        constrained->child = padded;

        WidgetRef result = constrained;

        // Tap / long-press
        if ((cfg.on_tap || cfg.on_long_press) && cfg.enabled) {
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap      = cfg.on_tap;
            gesture->on_long_press = cfg.on_long_press;
            gesture->child       = result;
            result               = gesture;
        }

        // Disabled state
        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = result;
            return faded;
        }

        return result;
    }

    // -----------------------------------------------------------------------
    // Divider
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildDivider(const DividerConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto line = std::make_shared<Container>();
        line->height = 1.0f;
        line->color  = c.outline;

        auto indented = std::make_shared<Padding>();
        indented->padding = EdgeInsets::only(cfg.indent, 0.0f, cfg.end_indent, 0.0f);
        indented->child   = line;

        auto box = std::make_shared<SizedBox>();
        box->height = 16.0f;
        box->child  = std::make_shared<Align>(Alignment::center(), indented);
        return box;
    }


    // -----------------------------------------------------------------------
    // AppBar
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildAppBar(const AppBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        // Compose row children: [leading] [title (expanded)] [actions...]
        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(12.0f));
        }

        if (cfg.title) {
            auto title_widget = cfg.title;
            if (cfg.center_title) {
                row_children.push_back(std::make_shared<Expanded>(WidgetRef(title_widget)));
            } else {
                row_children.push_back(std::make_shared<Expanded>(WidgetRef(title_widget)));
            }
        } else {
            row_children.push_back(std::make_shared<Expanded>(
                WidgetRef(SizedBox::shrink())));
        }

        for (const auto& action : cfg.actions) {
            row_children.push_back(SizedBox::from_width(8.0f));
            row_children.push_back(action);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
        padded->child   = row;

        auto container = std::make_shared<Container>();
        container->color = c.surface;
        container->child = padded;

        return container;
    }

    // -----------------------------------------------------------------------
    // NavigationBar
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildNavigationBar(const NavigationBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;

            // Icon + label column
            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::center;

            if (item.icon) {
                col->children.push_back(item.icon);
            }
            if (!item.label.empty()) {
                TextStyle ts;
                ts.font_size = 12.0f;
                ts.color = selected ? c.primary : c.on_surface_variant;
                if (selected) ts.font_weight = FontWeight::bold;
                col->children.push_back(std::make_shared<Text>(item.label, ts));
            }

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(8.0f, 6.0f);
            padded->child   = col;

            if (cfg.on_tap) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_tap, idx = static_cast<int>(i)]() { cb(idx); };
                gesture->child  = padded;
                item_widgets.push_back(std::make_shared<Expanded>(WidgetRef(gesture)));
            } else {
                item_widgets.push_back(std::make_shared<Expanded>(WidgetRef(padded)));
            }
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(item_widgets);

        auto container = std::make_shared<Container>();
        container->color = c.surface;
        container->child = row;

        return container;
    }

    // -----------------------------------------------------------------------
    // Dialog
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildDialog(const DialogConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<WidgetRef> children;

        // Title
        if (cfg.title) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(24.0f, 20.0f, 24.0f, 0.0f);
            padded->child   = cfg.title;
            children.push_back(padded);
        }

        // Content
        if (cfg.content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(24.0f, 16.0f, 24.0f, 0.0f);
            padded->child   = cfg.content;
            children.push_back(padded);
        }

        // Spacer + actions
        if (!cfg.actions.empty()) {
            children.push_back(SizedBox::from_height(20.0f));
            auto row = std::make_shared<Row>();
            row->main_axis_alignment = MainAxisAlignment::end;
            row->children = cfg.actions;
            auto action_pad = std::make_shared<Padding>();
            action_pad->padding = EdgeInsets::only(16.0f, 0.0f, 16.0f, 16.0f);
            action_pad->child   = row;
            children.push_back(action_pad);
        } else {
            children.push_back(SizedBox::from_height(20.0f));
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        auto dialog = std::make_shared<Dialog>();
        dialog->child = col;
        dialog->background_color = c.surface;
        dialog->border_radius = tokens_.shape.radius_lg; // 12 px
        dialog->elevation = tokens_.elevation.level4;    // 8 px
        dialog->max_width = 480.0f;
        return dialog;
    }

    // -----------------------------------------------------------------------
    // SnackBar
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildSnackBar(const SnackBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        TextStyle msg_style;
        msg_style.font_size = 14.0f;
        msg_style.color = c.on_surface;
        auto msg_text = std::make_shared<Text>(cfg.message, msg_style);

        std::vector<WidgetRef> row_children;
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(msg_text)));

        if (cfg.action_label.has_value() && cfg.on_action) {
            TextStyle action_style;
            action_style.font_size = 14.0f;
            action_style.color = c.primary;
            action_style.font_weight = FontWeight::bold;
            auto action_text = std::make_shared<Text>(*cfg.action_label, action_style);

            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = cfg.on_action;
            gesture->child  = action_text;
            row_children.push_back(SizedBox::from_width(16.0f));
            row_children.push_back(gesture);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto sb = std::make_shared<SnackBar>();
        sb->content = row;
        sb->background_color = c.inverse_surface;
        sb->border_radius = tokens_.shape.radius_md; // 8 px
        sb->padding = EdgeInsets::symmetric(16.0f, 14.0f);
        sb->duration_ms = cfg.duration_ms;
        return sb;
    }


    // -----------------------------------------------------------------------
    // PopupMenuButton
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildPopupMenuButton(const PopupMenuConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<PopupMenuItem> items;
        for (const auto& item : cfg.items) {
            PopupMenuItem pmi;
            pmi.label   = item.label;
            pmi.enabled = true;
            pmi.on_tap  = item.on_selected;
            if (!item.label.empty()) {
                TextStyle ts;
                ts.font_size = 14.0f;
                ts.color = c.on_surface;
                pmi.child = std::make_shared<Text>(item.label, ts);
            }
            items.push_back(std::move(pmi));
        }

        auto pmb = std::make_shared<PopupMenuButton>();
        pmb->items = std::move(items);
        pmb->on_selected = cfg.on_selected;
        pmb->child = cfg.child;
        pmb->popup_color   = c.surface;
        pmb->border_radius = tokens_.shape.radius_md; // 8 px
        pmb->elevation     = tokens_.elevation.level3; // 6 px
        return pmb;
    }

    // -----------------------------------------------------------------------
    // DropdownButton
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildDropdownButton(const DropdownConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<DropdownMenuItem<std::string>> items;
        for (const auto& item : cfg.items) {
            DropdownMenuItem<std::string> dmi;
            dmi.value = item.value;
            dmi.enabled = true;
            TextStyle ts;
            ts.font_size = 14.0f;
            ts.color = c.on_surface;
            dmi.child = std::make_shared<Text>(item.label, ts);
            items.push_back(std::move(dmi));
        }

        auto dd = std::make_shared<DropdownButton<std::string>>();
        dd->items = std::move(items);
        dd->hint  = cfg.hint;
        if (cfg.selected_value.has_value())
            dd->value = *cfg.selected_value;
        if (cfg.on_changed)
            dd->on_changed = cfg.on_changed;
        dd->dropdown_color = c.surface;
        dd->border_radius  = tokens_.shape.radius_md; // 8 px
        dd->elevation      = tokens_.elevation.level3; // 6 px
        return dd;
    }

    // -----------------------------------------------------------------------
    // PrimaryActionButton (FAB)
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildPrimaryActionButton(const PrimaryActionConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        const float diameter = 56.0f;

        BoxDecoration deco;
        deco.color         = c.primary;
        deco.border_radius = diameter / 2.0f; // circular
        float el = tokens_.elevation.level3;
        if (el > 0.0f) {
            deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                    Offset{0.0f, el * 0.5f},
                    el * 2.0f
                }
            };
        }

        WidgetRef content;
        if (cfg.icon) {
            content = cfg.icon;
        } else if (cfg.label) {
            content = cfg.label;
        } else {
            content = std::make_shared<Text>("+",
                TextStyle{}.withFontSize(24.0f).withColor(c.on_primary));
        }

        // Align must be the *inner* widget (centering content within the
        // box) with SizedBox outermost (pinning the box itself to a fixed
        // 56x56) — not the other way around. Align sizes itself to fill
        // whatever bounded space its parent gives it (correct, standard
        // Align semantics) and only positions its child within that area;
        // wrapping an already-56x56 SizedBox in an outer Align doesn't
        // constrain anything; the whole thing expands to fill its own
        // parent instead of staying pinned to 56x56. Never caught before
        // because this builder had never actually been exercised inside a
        // real layout (only unit-tested for non-null return) until it was
        // first rendered in examples/gallery — see TODO.md.
        auto centered = std::make_shared<Align>(Alignment::center(), content);
        auto sized    = SizedBox::square(diameter, centered);

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = sized;

        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
        gesture->child  = decorated;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = gesture;
            return faded;
        }
        return gesture;
    }

    // -----------------------------------------------------------------------
    // TabBar
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildTabBar(const TabBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<Tab> tabs;
        for (const auto& t : cfg.tabs) {
            Tab tab;
            tab.text = t.label;
            tab.icon = t.icon;
            tabs.push_back(std::move(tab));
        }

        auto tb = std::make_shared<TabBar>();
        tb->tabs = std::move(tabs);
        tb->indicator_color = c.primary;
        tb->label_color = c.primary;
        tb->unselected_label_color = c.on_surface_variant;
        tb->indicator_weight = 2.0f;
        tb->background_color = c.surface;
        return tb;
    }

    // -----------------------------------------------------------------------
    // Chip
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildChip(const ChipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        Color bg     = cfg.selected ? c.primary_container : c.surface_variant;
        Color border = cfg.selected ? c.primary : Color::transparent();
        Color fg     = cfg.selected ? c.on_primary_container : c.on_surface_variant;

        std::vector<WidgetRef> row_children;
        if (cfg.leading_icon) {
            row_children.push_back(cfg.leading_icon);
            row_children.push_back(SizedBox::from_width(6.0f));
        }
        row_children.push_back(cfg.label);
        if (cfg.on_deleted) {
            row_children.push_back(SizedBox::from_width(6.0f));
            auto del = std::make_shared<Text>("x", TextStyle{}.withFontSize(12.0f).withColor(fg));
            auto del_gesture = std::make_shared<GestureDetector>();
            del_gesture->on_tap = cfg.on_deleted;
            del_gesture->child  = del;
            row_children.push_back(del_gesture);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->main_axis_size = MainAxisSize::min;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(12.0f, 6.0f);
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = bg;
        deco.border_radius = tokens_.shape.radius_full; // pill, campello_ui's chip shape
        if (cfg.selected) deco.border = BoxBorder::all(border, 1.0f);

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = (cfg.enabled && cfg.on_selected) ? cfg.on_selected : nullptr;
        gesture->child  = decorated;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = gesture;
            return faded;
        }
        return gesture;
    }

    // -----------------------------------------------------------------------
    // SegmentedButton
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildSegmentedButton(const SegmentedConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> row_children;
        for (size_t i = 0; i < cfg.segments.size(); ++i) {
            const auto& seg = cfg.segments[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;

            std::vector<WidgetRef> content_children;
            if (seg.icon) content_children.push_back(seg.icon);
            if (seg.label) content_children.push_back(seg.label);
            auto row = std::make_shared<Row>();
            row->main_axis_alignment = MainAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            row->children = std::move(content_children);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(12.0f, 8.0f);
            padded->child   = row;

            BoxDecoration deco;
            deco.color         = selected ? c.primary : Color::transparent();
            deco.border_radius = tokens_.shape.radius_sm; // 4 px, inset from the pill container
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;

            WidgetRef item = decorated;
            if (cfg.on_changed && cfg.enabled) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_changed, idx = static_cast<int>(i)]() { cb(idx); };
                gesture->child  = decorated;
                item = gesture;
            }
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(item)));
        }

        auto container_row = std::make_shared<Row>();
        container_row->cross_axis_alignment = CrossAxisAlignment::stretch;
        container_row->children = std::move(row_children);

        auto padded_container = std::make_shared<Padding>();
        padded_container->padding = EdgeInsets::all(3.0f);
        padded_container->child   = container_row;

        BoxDecoration outer;
        outer.color         = c.surface_variant;
        outer.border_radius = tokens_.shape.radius_full;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = padded_container;

        // container_row's CrossAxisAlignment::stretch makes it report its
        // own height as whatever upper bound it's given — which, in a
        // loose/unbounded ancestor context (e.g. Expanded inside a Row
        // inside a Column with other flex siblings), can be enormous. Give
        // the whole control a real intrinsic height so it never depends on
        // the caller happening to provide a tightly-bounded one.
        auto sized = std::make_shared<SizedBox>();
        sized->height = 40.0f;
        sized->child  = decorated;

        WidgetRef result = sized;
        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = result;
            return faded;
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // BottomSheet
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildBottomSheet(const BottomSheetConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> children;

        if (cfg.show_drag_handle) {
            BoxDecoration handle_deco;
            handle_deco.color         = c.outline;
            handle_deco.border_radius = tokens_.shape.radius_full;
            auto handle_decorated = std::make_shared<DecoratedBox>();
            handle_decorated->decoration = handle_deco;
            auto sized_handle = std::make_shared<SizedBox>();
            sized_handle->width  = 32.0f;
            sized_handle->height = 4.0f;
            sized_handle->child  = handle_decorated;

            auto padded_handle = std::make_shared<Padding>();
            padded_handle->padding = EdgeInsets::symmetric(0.0f, 12.0f);
            padded_handle->child   = std::make_shared<Align>(Alignment::center(), sized_handle);
            children.push_back(padded_handle);
        }
        children.push_back(cfg.child);

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        BoxDecoration deco;
        deco.color         = c.surface;
        deco.border_radius = tokens_.shape.radius_lg; // 12 px — matches campello_ui's Dialog
        float el = tokens_.elevation.level4;
        if (el > 0.0f) {
            deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                    Offset{0.0f, -el * 0.25f},
                    el * 2.0f
                }
            };
        }
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // Badge
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildBadge(const BadgeConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        WidgetRef badge_content;
        float diameter = 8.0f;
        if (cfg.label.has_value()) {
            diameter = 16.0f;
            TextStyle ts;
            ts.font_size = 10.0f;
            ts.color = c.on_error;
            badge_content = std::make_shared<Text>(*cfg.label, ts);
        }

        BoxDecoration deco;
        deco.color         = c.error;
        deco.border_radius = tokens_.shape.radius_full;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        if (badge_content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(4.0f, 2.0f);
            padded->child   = badge_content;
            decorated->child = padded;
        }

        auto sized_badge = std::make_shared<SizedBox>();
        if (!cfg.label.has_value()) {
            sized_badge->width  = diameter;
            sized_badge->height = diameter;
        }
        sized_badge->child = decorated;

        auto positioned = std::make_shared<Positioned>();
        positioned->right = -4.0f;
        positioned->top   = -4.0f;
        positioned->child = sized_badge;

        auto stack = std::make_shared<Stack>();
        stack->children = {cfg.child, positioned};
        return stack;
    }

    // -----------------------------------------------------------------------
    // IconButton
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildIconButton(const IconButtonConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::all(8.0f);
        padded->child   = cfg.icon;

        BoxDecoration deco;
        deco.color         = cfg.selected ? c.primary_container : Color::transparent();
        deco.border_radius = tokens_.shape.radius_full;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
        gesture->child  = decorated;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = gesture;
            return faded;
        }
        return gesture;
    }

    // -----------------------------------------------------------------------
    // Stepper
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildStepper(const StepperConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto makeButton = [&](const std::string& glyph, std::function<void()> on_tap) {
            BoxDecoration deco;
            deco.color         = c.surface_variant;
            deco.border_radius = tokens_.shape.radius_sm;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(10.0f, 6.0f);
            padded->child   = std::make_shared<Text>(glyph,
                TextStyle{}.withFontSize(16.0f).withColor(c.primary));
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = std::move(on_tap);
            gesture->child  = decorated;
            return gesture;
        };

        const bool can_dec = cfg.enabled && cfg.on_changed && cfg.value > cfg.min;
        const bool can_inc = cfg.enabled && cfg.on_changed && cfg.value < cfg.max;

        auto dec = makeButton("-", can_dec
            ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, lo = cfg.min] {
                  cb(std::max(lo, v - step));
              })
            : nullptr);
        auto inc = makeButton("+", can_inc
            ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, hi = cfg.max] {
                  cb(std::min(hi, v + step));
              })
            : nullptr);

        auto value_text = std::make_shared<Text>(std::to_string(cfg.value),
            TextStyle{}.withFontSize(14.0f).withColor(c.on_surface));
        auto padded_value = std::make_shared<Padding>();
        padded_value->padding = EdgeInsets::symmetric(12.0f, 6.0f);
        padded_value->child   = value_text;

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->main_axis_size = MainAxisSize::min;
        row->children = {dec, padded_value, inc};

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = row;
            return faded;
        }
        return row;
    }

    // -----------------------------------------------------------------------
    // RatingIndicator
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildRatingIndicator(const RatingConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> stars;
        for (int i = 0; i < cfg.max; ++i) {
            bool filled = i < cfg.value;
            auto glyph = std::make_shared<Text>(filled ? "*" : "-",
                TextStyle{}.withFontSize(18.0f).withColor(filled ? c.secondary : c.outline));
            WidgetRef star = glyph;
            if (cfg.enabled && cfg.on_changed) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_changed, idx = i] { cb(idx + 1); };
                gesture->child  = glyph;
                star = gesture;
            }
            stars.push_back(star);
            if (i + 1 < cfg.max) stars.push_back(SizedBox::from_width(2.0f));
        }

        auto row = std::make_shared<Row>();
        row->main_axis_size = MainAxisSize::min;
        row->children = std::move(stars);

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = row;
            return faded;
        }
        return row;
    }

    // -----------------------------------------------------------------------
    // ActionSheet
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildActionSheet(const ActionSheetConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> children;

        if (cfg.title) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::all(16.0f);
            padded->child   = cfg.title;
            children.push_back(padded);
        }

        for (const auto& action : cfg.actions) {
            TextStyle ts;
            ts.font_size = 14.0f;
            ts.color = action.destructive ? c.error : c.on_surface;
            auto label = std::make_shared<Text>(action.label, ts);
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 14.0f);
            padded->child   = label;
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = action.on_selected;
            gesture->child  = padded;
            children.push_back(gesture);

            auto line = std::make_shared<Container>();
            line->height = 1.0f;
            line->color  = c.outline_variant;
            children.push_back(line);
        }

        if (cfg.on_cancel) {
            children.push_back(SizedBox::from_height(8.0f));
            auto label = std::make_shared<Text>("Cancel",
                TextStyle{}.withFontSize(14.0f).bold().withColor(c.primary));
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 14.0f);
            padded->child   = label;
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = cfg.on_cancel;
            gesture->child  = padded;
            children.push_back(gesture);
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        BoxDecoration deco;
        deco.color         = c.surface;
        deco.border_radius = tokens_.shape.radius_lg; // 12 px, matches Dialog/BottomSheet
        float el = tokens_.elevation.level4;
        if (el > 0.0f) {
            deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                    Offset{0.0f, -el * 0.25f},
                    el * 2.0f
                }
            };
        }
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // SearchField
    // -----------------------------------------------------------------------

    WidgetRef CampelloDesignSystem::buildSearchField(const SearchFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto icon = std::make_shared<Text>("search",
            TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));

        auto controller = std::make_shared<TextEditingController>(cfg.value);
        auto tf = std::make_shared<TextField>(controller, cfg.placeholder);
        tf->fill_color        = Color::transparent();
        tf->border_color      = Color::transparent();
        tf->placeholder_color = c.on_surface_variant;
        tf->min_height        = 40.0f;
        if (cfg.on_changed)
            tf->on_changed = [cb = cfg.on_changed](const std::string& v) { cb(v); };

        std::vector<WidgetRef> row_children;
        row_children.push_back(icon);
        row_children.push_back(SizedBox::from_width(8.0f));
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(tf)));
        if (!cfg.value.empty() && cfg.on_clear) {
            auto clear = std::make_shared<Text>("x",
                TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = cfg.on_clear;
            gesture->child  = clear;
            row_children.push_back(SizedBox::from_width(8.0f));
            row_children.push_back(gesture);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(14.0f, 4.0f);
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = c.surface_variant;
        deco.border_radius = tokens_.shape.radius_full; // pill search field
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = decorated;
            return faded;
        }
        return decorated;
    }

    // -----------------------------------------------------------------------
    // DatePicker / TimePicker — tappable trigger fields (see doc comment on
    // DatePickerConfig: this builds chrome only, not calendar/wheel UI).
    // -----------------------------------------------------------------------

    namespace
    {
        WidgetRef buildTriggerField(const DesignTokens& tokens, const std::string& glyph,
                                     const std::string& label, std::function<void()> on_tap,
                                     bool enabled)
        {
            const auto& c = tokens.colors;
            auto icon_text = std::make_shared<Text>(glyph,
                TextStyle{}.withFontSize(12.0f).withColor(c.primary));
            auto label_text = std::make_shared<Text>(label,
                TextStyle{}.withFontSize(14.0f).withColor(c.on_surface));

            auto row = std::make_shared<Row>();
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            row->children = {icon_text, SizedBox::from_width(8.0f), label_text};

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(14.0f, 10.0f);
            padded->child   = row;

            BoxDecoration deco;
            deco.color         = tokens.colors.surface_variant;
            deco.border_radius = tokens.shape.radius_md;
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;

            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = (enabled && on_tap) ? std::move(on_tap) : nullptr;
            gesture->child  = decorated;

            if (!enabled) {
                auto faded = std::make_shared<Opacity>();
                faded->opacity = 0.4f;
                faded->child   = gesture;
                return faded;
            }
            return gesture;
        }
    } // namespace

    WidgetRef CampelloDesignSystem::buildDatePicker(const DatePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "date", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef CampelloDesignSystem::buildTimePicker(const TimePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "time", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef CampelloDesignSystem::buildExpansionTile(const ExpansionTileConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<WidgetRef> header_children;
        if (cfg.leading) {
            header_children.push_back(cfg.leading);
            header_children.push_back(SizedBox::from_width(12.0f));
        }
        WidgetRef title_section = cfg.title;
        if (cfg.subtitle) {
            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::start;
            col->children = {cfg.title, cfg.subtitle};
            title_section = col;
        }
        header_children.push_back(std::make_shared<Expanded>(WidgetRef(title_section)));
        header_children.push_back(std::make_shared<Text>(cfg.expanded ? "^" : "v",
            TextStyle{}.withFontSize(13.0f).withColor(c.primary)));

        auto header_row = std::make_shared<Row>();
        header_row->cross_axis_alignment = CrossAxisAlignment::center;
        header_row->children = std::move(header_children);

        auto header_padded = std::make_shared<Padding>();
        header_padded->padding = EdgeInsets::symmetric(16.0f, 14.0f);
        header_padded->child   = header_row;

        BoxDecoration header_deco;
        header_deco.color         = cfg.expanded ? c.surface_variant : Color::transparent();
        header_deco.border_radius = tokens_.shape.radius_md;
        auto header_decorated = std::make_shared<DecoratedBox>();
        header_decorated->decoration = header_deco;
        header_decorated->child      = header_padded;

        auto header_gesture = std::make_shared<GestureDetector>();
        if (cfg.enabled && cfg.on_expansion_changed) {
            header_gesture->on_tap = [cb = cfg.on_expansion_changed, expanded = cfg.expanded] { cb(!expanded); };
        }
        header_gesture->child = header_decorated;

        std::vector<WidgetRef> children = {header_gesture};
        if (cfg.expanded && cfg.children_content) {
            auto body_padded = std::make_shared<Padding>();
            body_padded->padding = EdgeInsets::symmetric(16.0f, 8.0f);
            body_padded->child   = cfg.children_content;
            children.push_back(body_padded);
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = col;
            return faded;
        }
        return col;
    }

    WidgetRef CampelloDesignSystem::buildToggleButtons(const ToggleButtonsConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> items;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];

            std::vector<WidgetRef> content_children;
            if (item.icon) content_children.push_back(item.icon);
            if (item.label) content_children.push_back(item.label);
            auto content_row = std::make_shared<Row>();
            content_row->main_axis_alignment = MainAxisAlignment::center;
            content_row->main_axis_size = MainAxisSize::min;
            content_row->children = std::move(content_children);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(14.0f, 8.0f);
            padded->child   = content_row;

            BoxDecoration deco;
            deco.color         = item.selected ? c.primary_container : Color::transparent();
            deco.border_radius = tokens_.shape.radius_md;
            deco.border        = BoxBorder::all(c.outline, 1.0f);
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;

            WidgetRef btn = decorated;
            if (cfg.enabled && cfg.on_pressed) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_pressed, idx = static_cast<int>(i)] { cb(idx); };
                gesture->child  = decorated;
                btn = gesture;
            }
            items.push_back(btn);
            if (i + 1 < cfg.items.size()) items.push_back(SizedBox::from_width(8.0f));
        }

        auto row = std::make_shared<Row>();
        row->main_axis_size = MainAxisSize::min;
        row->children = std::move(items);

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = row;
            return faded;
        }
        return row;
    }

    WidgetRef CampelloDesignSystem::buildBanner(const BannerConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(12.0f));
        }
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(cfg.content)));
        for (const auto& action : cfg.actions) {
            row_children.push_back(SizedBox::from_width(8.0f));
            row_children.push_back(action);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::all(16.0f);
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = c.surface_variant;
        deco.border_radius = tokens_.shape.radius_md;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;
        return decorated;
    }

    WidgetRef CampelloDesignSystem::buildNavigationRail(const NavigationRailConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;
            Color tint = selected ? c.primary : c.on_surface_variant;

            std::vector<WidgetRef> content;
            if (item.icon) content.push_back(item.icon);
            if (cfg.extended && !item.label.empty()) {
                content.push_back(SizedBox::from_width(12.0f));
                content.push_back(std::make_shared<Text>(item.label,
                    TextStyle{}.withFontSize(13.0f).withColor(tint)));
            }

            auto row = std::make_shared<Row>();
            row->main_axis_alignment = cfg.extended ? MainAxisAlignment::start : MainAxisAlignment::center;
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->children = std::move(content);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(cfg.extended ? 16.0f : 10.0f, 12.0f);
            padded->child   = row;

            BoxDecoration deco;
            deco.color         = selected ? c.primary_container : Color::transparent();
            deco.border_radius = tokens_.shape.radius_full;
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;

            WidgetRef entry = decorated;
            if (cfg.on_tap) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_tap, idx = static_cast<int>(i)] { cb(idx); };
                gesture->child  = decorated;
                entry = gesture;
            }
            item_widgets.push_back(entry);
            if (i + 1 < cfg.items.size()) item_widgets.push_back(SizedBox::from_height(6.0f));
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = cfg.extended ? CrossAxisAlignment::stretch : CrossAxisAlignment::center;
        col->children = std::move(item_widgets);

        auto padded_col = std::make_shared<Padding>();
        padded_col->padding = EdgeInsets::symmetric(8.0f, 12.0f);
        padded_col->child   = col;

        BoxDecoration outer;
        outer.color = c.surface;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = padded_col;
        return decorated;
    }

    WidgetRef CampelloDesignSystem::buildDataTable(const DataTableConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto makeCell = [&](WidgetRef content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(14.0f, 10.0f);
            padded->child   = std::move(content);
            return std::make_shared<Expanded>(WidgetRef(padded));
        };

        std::vector<WidgetRef> header_cells;
        for (const auto& col_name : cfg.columns) {
            header_cells.push_back(makeCell(std::make_shared<Text>(col_name,
                TextStyle{}.withFontSize(13.0f).bold().withColor(c.on_surface_variant))));
        }
        auto header_row = std::make_shared<Row>();
        header_row->children = std::move(header_cells);

        BoxDecoration header_deco;
        header_deco.color = c.surface_variant;
        auto header_decorated = std::make_shared<DecoratedBox>();
        header_decorated->decoration = header_deco;
        header_decorated->child      = header_row;

        std::vector<WidgetRef> table_children = {header_decorated};
        for (const auto& data_row : cfg.rows) {
            auto divider = std::make_shared<Container>();
            divider->height = 1.0f;
            divider->color  = c.outline_variant;
            table_children.push_back(divider);

            std::vector<WidgetRef> cells;
            for (const auto& cell : data_row) cells.push_back(makeCell(cell));
            auto row = std::make_shared<Row>();
            row->children = std::move(cells);
            table_children.push_back(row);
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(table_children);

        BoxDecoration outer;
        outer.border        = BoxBorder::all(c.outline, 1.0f);
        outer.border_radius = tokens_.shape.radius_md;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = col;
        return decorated;
    }

} // namespace systems::leal::campello_widgets
