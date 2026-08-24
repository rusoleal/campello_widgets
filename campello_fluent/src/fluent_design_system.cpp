#include <campello_fluent/fluent_design_system.hpp>

#include "fluent_reveal_response.hpp"

// Widgets
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/dropdown_button.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/linear_progress_indicator.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/popup_menu_button.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/radio.hpp>
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

// UI primitives
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/text_style.hpp>

#include <algorithm>
#include <limits>

namespace systems::leal::campello_widgets
{

namespace
{

Color fluentBlue()      { return Color::fromARGB(0xFF0078D4); }
Color fluentLightBg()   { return Color::fromARGB(0xFFF3F3F3); }
Color fluentLightSurface() { return Color::fromARGB(0xFFFFFFFF); }
Color fluentDarkBg()    { return Color::fromARGB(0xFF202020); }
// Was 0x1C1C1C — darker than fluentDarkBg(), backwards from Fluent 2's dark
// elevation convention (an elevated surface should be lighter than the page
// behind it, mirroring fluentLightSurface() being lighter than
// fluentLightBg()). Confirmed against a real WinUI3 CardBackgroundFillColorDefault
// sample (measured (43,43,43) ≈ 0x2B2B2B via windows_fidelity_reference) —
// card_filled's diff was dominated by this single wrong token (~17 avg
// delta across the whole card area), not text color as first suspected.
Color fluentDarkSurface()  { return Color::fromARGB(0xFF2B2B2B); }

TypographyScale makeFluentTypography(Color foreground)
{
    TypographyScale t;
    // Fluent type ramp mapped onto the 15-role scale.
    // Real Fluent uses Segoe UI Variable; this uses the framework default font
    // with size/weight values from the Fluent 2 ramp.
    t.display_large   = TextStyle{}.withFontSize(68.0f).withColor(foreground);
    t.display_medium  = TextStyle{}.withFontSize(54.0f).withColor(foreground);
    t.display_small   = TextStyle{}.withFontSize(44.0f).withColor(foreground);
    t.headline_large  = TextStyle{}.withFontSize(34.0f).bold().withColor(foreground);
    t.headline_medium = TextStyle{}.withFontSize(28.0f).bold().withColor(foreground);
    t.headline_small  = TextStyle{}.withFontSize(24.0f).bold().withColor(foreground);
    t.title_large     = TextStyle{}.withFontSize(20.0f).bold().withColor(foreground);
    t.title_medium    = TextStyle{}.withFontSize(16.0f).bold().withColor(foreground);
    t.title_small     = TextStyle{}.withFontSize(14.0f).bold().withColor(foreground);
    t.body_large      = TextStyle{}.withFontSize(16.0f).withColor(foreground);
    t.body_medium     = TextStyle{}.withFontSize(14.0f).withColor(foreground);
    t.body_small      = TextStyle{}.withFontSize(12.0f).withColor(foreground);
    t.label_large     = TextStyle{}.withFontSize(14.0f).withColor(foreground);
    t.label_medium    = TextStyle{}.withFontSize(12.0f).withColor(foreground);
    t.label_small     = TextStyle{}.withFontSize(10.0f).withColor(foreground);
    return t;
}

DesignTokens makeLightTokens(Color accent)
{
    DesignTokens t;
    t.brightness = Brightness::light;

    t.colors.primary            = accent;
    t.colors.on_primary         = Color::fromARGB(0xFFFFFFFF);
    t.colors.primary_container  = FluentDesignSystem::withOpacity(accent, 0.12f);
    t.colors.on_primary_container = accent;
    t.colors.secondary          = Color::fromARGB(0xFF333333);
    t.colors.on_secondary       = Color::fromARGB(0xFFFFFFFF);
    t.colors.secondary_container = Color::fromARGB(0xFFF0F0F0);
    t.colors.on_secondary_container = Color::fromARGB(0xFF1A1A1A);
    t.colors.tertiary           = Color::fromARGB(0xFF596566);
    t.colors.on_tertiary        = Color::fromARGB(0xFFFFFFFF);
    t.colors.tertiary_container = Color::fromARGB(0xFFE6E6E6);
    t.colors.on_tertiary_container = Color::fromARGB(0xFF1A1A1A);
    t.colors.surface            = fluentLightSurface();
    t.colors.on_surface         = Color::fromARGB(0xFF1A1A1A);
    t.colors.surface_variant    = Color::fromARGB(0xFFF0F0F0);
    t.colors.on_surface_variant = Color::fromARGB(0xFF5F5F5F);
    t.colors.background         = fluentLightBg();
    t.colors.on_background      = Color::fromARGB(0xFF1A1A1A);
    t.colors.error              = Color::fromARGB(0xFFC42B1C);
    t.colors.on_error           = Color::fromARGB(0xFFFFFFFF);
    t.colors.error_container    = Color::fromARGB(0xFFFFDEDB);
    t.colors.on_error_container = Color::fromARGB(0xFF6F0D0D);
    t.colors.success            = Color::fromARGB(0xFF0F7B0F);
    t.colors.on_success         = Color::fromARGB(0xFFFFFFFF);
    t.colors.warning            = Color::fromARGB(0xFF9D5D00);
    t.colors.on_warning         = Color::fromARGB(0xFFFFFFFF);
    t.colors.outline            = Color::fromARGB(0xFF8A8A8A);
    t.colors.outline_variant    = Color::fromARGB(0xFFE0E0E0);
    t.colors.shadow             = Color::fromARGB(0xFF000000);
    t.colors.scrim              = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.3f);
    t.colors.inverse_surface    = Color::fromARGB(0xFF2B2B2B);
    t.colors.inverse_on_surface = Color::fromARGB(0xFFFFFFFF);
    t.colors.inverse_primary    = Color::fromARGB(0xFFCCE3F1);

    // Fluent elevation is subtle: mostly 0/1/2/4/8/16 px shadow depths.
    t.elevation = ElevationTokens{0.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};

    // Fluent 2 shape scale: 0/2/4/8/12/16 dp, with full rounding for pills.
    t.shape.radius_none = 0.0f;
    t.shape.radius_xs   = 2.0f;
    t.shape.radius_sm   = 4.0f;
    t.shape.radius_md   = 4.0f;
    t.shape.radius_lg   = 8.0f;
    t.shape.radius_xl   = 16.0f;
    t.shape.radius_full = 9999.0f;

    // Fluent spacing is compact; comfortable mode would bump these up.
    t.spacing.xs   = 4.0f;
    t.spacing.sm   = 8.0f;
    t.spacing.md   = 12.0f;
    t.spacing.lg   = 16.0f;
    t.spacing.xl   = 20.0f;
    t.spacing.xxl  = 24.0f;
    t.spacing.xxxl = 32.0f;

    // Fluent motion: standard 100ms, ease-in-out for UI transitions.
    t.motion.duration_instant = 0.0;
    t.motion.duration_fast    = 100.0;
    t.motion.duration_normal  = 200.0;
    t.motion.duration_slow    = 300.0;
    t.motion.duration_slower  = 500.0;
    t.motion.curve_standard   = Curves::easeInOut;
    t.motion.curve_decelerate = Curves::easeOut;
    t.motion.curve_accelerate = Curves::easeIn;
    t.motion.curve_emphasized = Curves::easeInOut;

    t.typography = makeFluentTypography(t.colors.on_surface);
    return t;
}

DesignTokens makeDarkTokens(Color accent)
{
    DesignTokens t = makeLightTokens(accent);
    t.brightness = Brightness::dark;

    t.colors.surface            = fluentDarkSurface();
    t.colors.on_surface         = Color::fromARGB(0xFFFFFFFF);
    t.colors.surface_variant    = Color::fromARGB(0xFF2C2C2C);
    t.colors.on_surface_variant = Color::fromARGB(0xFFB3B3B3);
    t.colors.background         = fluentDarkBg();
    t.colors.on_background      = Color::fromARGB(0xFFFFFFFF);
    t.colors.primary_container  = FluentDesignSystem::withOpacity(accent, 0.20f);
    t.colors.on_primary_container = accent;
    t.colors.secondary_container = Color::fromARGB(0xFF2D2D2D);
    t.colors.on_secondary_container = Color::fromARGB(0xFFFFFFFF);
    t.colors.tertiary_container = Color::fromARGB(0xFF333333);
    t.colors.on_tertiary_container = Color::fromARGB(0xFFFFFFFF);
    t.colors.error_container    = Color::fromARGB(0xFF4A1C18);
    t.colors.on_error_container = Color::fromARGB(0xFFFFDEDB);
    t.colors.outline            = Color::fromARGB(0xFF6E6E6E);
    t.colors.outline_variant    = Color::fromARGB(0xFF3F3F3F);
    t.colors.inverse_surface    = Color::fromARGB(0xFFF3F3F3);
    t.colors.inverse_on_surface = Color::fromARGB(0xFF1A1A1A);
    t.colors.inverse_primary    = accent;

    t.typography = makeFluentTypography(t.colors.on_surface);
    return t;
}

DesignTokens makeHighContrastTokens(Color accent)
{
    // High contrast is deliberately simple: black/white/accent, no grays.
    DesignTokens t;
    t.brightness = Brightness::light;

    t.colors.primary            = accent;
    t.colors.on_primary         = Color::fromARGB(0xFF000000);
    t.colors.primary_container  = Color::fromARGB(0xFFFFFFFF);
    t.colors.on_primary_container = Color::fromARGB(0xFF000000);
    t.colors.secondary          = Color::fromARGB(0xFF000000);
    t.colors.on_secondary       = Color::fromARGB(0xFFFFFFFF);
    t.colors.secondary_container = Color::fromARGB(0xFFFFFFFF);
    t.colors.on_secondary_container = Color::fromARGB(0xFF000000);
    t.colors.tertiary           = Color::fromARGB(0xFF000000);
    t.colors.on_tertiary        = Color::fromARGB(0xFFFFFFFF);
    t.colors.tertiary_container = Color::fromARGB(0xFFFFFFFF);
    t.colors.on_tertiary_container = Color::fromARGB(0xFF000000);
    t.colors.surface            = Color::fromARGB(0xFF000000);
    t.colors.on_surface         = Color::fromARGB(0xFFFFFFFF);
    t.colors.surface_variant    = Color::fromARGB(0xFF000000);
    t.colors.on_surface_variant = Color::fromARGB(0xFFFFFFFF);
    t.colors.background         = Color::fromARGB(0xFF000000);
    t.colors.on_background      = Color::fromARGB(0xFFFFFFFF);
    t.colors.error              = Color::fromARGB(0xFFFFA2A2);
    t.colors.on_error           = Color::fromARGB(0xFF000000);
    t.colors.error_container    = Color::fromARGB(0xFFFFFFFF);
    t.colors.on_error_container = Color::fromARGB(0xFF000000);
    t.colors.success            = Color::fromARGB(0xFF7DFF7D);
    t.colors.on_success         = Color::fromARGB(0xFF000000);
    t.colors.warning            = Color::fromARGB(0xFFFFFFA2);
    t.colors.on_warning         = Color::fromARGB(0xFF000000);
    t.colors.outline            = Color::fromARGB(0xFFFFFFFF);
    t.colors.outline_variant    = Color::fromARGB(0xFFFFFFFF);
    t.colors.shadow             = Color::fromARGB(0xFF000000);
    t.colors.scrim              = Color::fromARGB(0xFF000000);
    t.colors.inverse_surface    = Color::fromARGB(0xFFFFFFFF);
    t.colors.inverse_on_surface = Color::fromARGB(0xFF000000);
    t.colors.inverse_primary    = accent;

    t.elevation = ElevationTokens{0.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    t.shape.radius_none = 0.0f;
    t.shape.radius_xs   = 0.0f;
    t.shape.radius_sm   = 0.0f;
    t.shape.radius_md   = 0.0f;
    t.shape.radius_lg   = 0.0f;
    t.shape.radius_xl   = 0.0f;
    t.shape.radius_full = 9999.0f;

    t.typography = makeFluentTypography(t.colors.on_surface);
    return t;
}

} // namespace

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

Color FluentDesignSystem::withOpacity(Color c, float opacity)
{
    return Color::fromRGBA(c.r, c.g, c.b, c.a * opacity);
}

FluentDesignSystem::FluentDesignSystem()
    : FluentDesignSystem(makeLightTokens(fluentBlue()), fluentBlue())
{
}

FluentDesignSystem::FluentDesignSystem(DesignTokens tokens, Color accent)
    : tokens_(std::move(tokens)), accent_(accent)
{
}

FluentDesignSystem FluentDesignSystem::light(Color accent)
{
    FluentDesignSystem ds(makeLightTokens(accent), accent);
    ds.material_ = FluentMaterial::mica;
    return ds;
}

FluentDesignSystem FluentDesignSystem::dark(Color accent)
{
    FluentDesignSystem ds(makeDarkTokens(accent), accent);
    ds.material_ = FluentMaterial::mica;
    return ds;
}

FluentDesignSystem FluentDesignSystem::highContrast(Color accent)
{
    FluentDesignSystem ds(makeHighContrastTokens(accent), accent);
    ds.material_ = FluentMaterial::smoke;
    return ds;
}

// ----------------------------------------------------------------------------
// Component builders
// ----------------------------------------------------------------------------

WidgetRef FluentDesignSystem::buildButton(const ButtonConfig& cfg) const
{
    const auto& c = tokens_.colors;

    // Fluent buttons: 4dp radius, 8/16dp padding, subtle border for standard.
    bool filled = (cfg.priority == ButtonPriority::primary);
    Color bg = filled ? accent_ : Color::fromARGB(0x00FFFFFF);
    Color fg = filled ? c.on_primary : c.on_surface;
    Color border = filled ? Color::fromARGB(0x00FFFFFF) : c.outline;

    WidgetRef content = cfg.label;
    if (cfg.leading_icon || cfg.trailing_icon) {
        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->main_axis_size = MainAxisSize::min;
        if (cfg.leading_icon) {
            row->children.push_back(cfg.leading_icon);
            row->children.push_back(std::make_shared<SizedBox>(tokens_.spacing.sm, 0.0f));
        }
        row->children.push_back(cfg.label);
        if (cfg.trailing_icon) {
            row->children.push_back(std::make_shared<SizedBox>(tokens_.spacing.sm, 0.0f));
            row->children.push_back(cfg.trailing_icon);
        }
        content = row;
    }

    auto padded = std::make_shared<Padding>();
    padded->padding = EdgeInsets::symmetric(24.0f, 10.0f);
    padded->child   = content;

    BoxDecoration deco;
    deco.color  = bg;
    deco.border = BoxBorder::all(border, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;
    auto box = std::make_shared<DecoratedBox>();
    box->decoration = deco;
    box->child      = padded;

    auto reveal = std::make_shared<FluentRevealResponse>();
    reveal->child         = box;
    reveal->on_tap        = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
    reveal->border_radius = tokens_.shape.radius_md; // matches deco.border_radius above
    reveal->reveal_color  = filled ? fg : accent_;
    reveal->focus_node    = cfg.focus_node;
    reveal->autofocus     = cfg.autofocus;

    if (!cfg.enabled || !cfg.on_pressed) {
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = reveal;
        return faded;
    }
    return reveal;
}

WidgetRef FluentDesignSystem::buildSwitch(const SwitchConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto sw = std::make_shared<Switch>();
    sw->value = cfg.value;
    sw->active_track_color   = accent_;
    sw->inactive_track_color = c.outline_variant;
    sw->active_thumb_color   = c.surface;
    sw->inactive_thumb_color = c.surface;

    if (cfg.enabled && cfg.on_changed) {
        sw->on_changed = cfg.on_changed;
        return sw;
    }
    sw->on_changed = nullptr;
    auto faded = std::make_shared<Opacity>();
    faded->opacity = 0.4f;
    faded->child   = sw;
    return faded;
}

WidgetRef FluentDesignSystem::buildCard(const CardConfig& cfg) const
{
    const auto& c = tokens_.colors;

    // Fluent cards: 8dp rounded corners. Priority changes fill/border, not
    // just elevation — outlined has no fill (border only), elevated drops
    // the border (depth comes from shadow instead), filled keeps both.
    BoxDecoration deco;
    deco.color = cfg.priority == CardPriority::outlined ? Color::fromARGB(0x00FFFFFF) : c.surface;
    if (cfg.priority != CardPriority::elevated)
        deco.border = BoxBorder::all(c.outline_variant, 1.0f);
    deco.border_radius = tokens_.shape.radius_lg;

    auto pad = std::make_shared<Padding>();
    pad->padding = cfg.padding;
    pad->child   = cfg.child ? cfg.child : std::make_shared<SizedBox>(0.0f, 0.0f);

    auto box = std::make_shared<DecoratedBox>();
    box->decoration = deco;
    box->child      = pad;
    return box;
}

WidgetRef FluentDesignSystem::buildCheckbox(const CheckboxConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto cb = std::make_shared<Checkbox>();
    cb->value = cfg.value;
    cb->active_color  = accent_;
    cb->check_color   = c.on_primary;
    cb->border_color  = c.outline;
    cb->border_radius = tokens_.shape.radius_xs;

    if (cfg.enabled && cfg.on_changed) {
        cb->on_changed = cfg.on_changed;
        return cb;
    }
    cb->on_changed = nullptr;
    auto faded = std::make_shared<Opacity>();
    faded->opacity = 0.4f;
    faded->child   = cb;
    return faded;
}

WidgetRef FluentDesignSystem::buildRadio(const RadioConfig& cfg) const
{
    const auto& c = tokens_.colors;
    // Radio in this framework is designed to live inside a RadioGroup. For a
    // single standalone radio, we render a simple circle placeholder.
    BoxDecoration deco;
    deco.color = cfg.selected ? accent_ : c.surface;
    deco.border = BoxBorder::all(c.outline, 2.0f);
    deco.border_radius = 9999.0f;

    auto circle = std::make_shared<DecoratedBox>();
    circle->decoration = deco;
    circle->child = std::make_shared<SizedBox>(20.0f, 20.0f);

    if (cfg.enabled && cfg.on_selected) {
        auto detector = std::make_shared<GestureDetector>();
        detector->on_tap = cfg.on_selected;
        detector->child  = circle;
        return detector;
    }
    auto faded = std::make_shared<Opacity>();
    faded->opacity = 0.4f;
    faded->child   = circle;
    return faded;
}

WidgetRef FluentDesignSystem::buildSlider(const SliderConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto slider = std::make_shared<Slider>();
    slider->value = cfg.value;
    slider->min   = cfg.min;
    slider->max   = cfg.max;
    slider->active_color   = accent_;
    slider->inactive_color = c.outline_variant;

    if (cfg.enabled && cfg.on_changed) {
        slider->on_changed = cfg.on_changed;
        return slider;
    }
    slider->on_changed = nullptr;
    auto faded = std::make_shared<Opacity>();
    faded->opacity = 0.4f;
    faded->child   = slider;
    return faded;
}

WidgetRef FluentDesignSystem::buildTextField(const TextFieldConfig& cfg) const
{
    const auto& c = tokens_.colors;

    auto field = std::make_shared<TextField>();
    field->placeholder = cfg.placeholder;
    field->obscure_text = cfg.obscure_text;
    field->max_lines    = cfg.max_lines;
    field->on_changed   = cfg.enabled ? cfg.on_changed : nullptr;
    field->on_submitted = cfg.enabled ? cfg.on_submitted : nullptr;
    field->cursor_color = accent_;
    field->style        = tokens_.typography.body_medium;
    field->placeholder_color = c.on_surface_variant;
    // TextField's own render object always paints its own fill/border
    // (defaulting to tokens->colors.surface/outline when unset) — leaving
    // these unset duplicates the outer DecoratedBox below as a second,
    // identical box. Matches buildSearchField()'s existing transparent-fill
    // convention for the same reason.
    field->fill_color   = Color::fromARGB(0x00FFFFFF);
    field->border_color = Color::fromARGB(0x00FFFFFF);

    if (!cfg.value.empty()) {
        field->controller = std::make_shared<TextEditingController>(cfg.value);
    }

    BoxDecoration deco;
    deco.color = c.surface;
    deco.border = BoxBorder::all(c.outline, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;

    auto pad = std::make_shared<Padding>();
    pad->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.sm);
    pad->child   = field;

    auto box = std::make_shared<DecoratedBox>();
    box->decoration = deco;
    box->child      = pad;

    if (!cfg.enabled) {
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = box;
        return faded;
    }
    return box;
}

WidgetRef FluentDesignSystem::buildProgressIndicator(const ProgressConfig& cfg) const
{
    const auto& c = tokens_.colors;
    if (cfg.type == ProgressType::linear) {
        auto ind = std::make_shared<LinearProgressIndicator>();
        ind->value = cfg.value;
        ind->value_color = accent_;
        ind->background_color = c.outline_variant;
        return ind;
    }
    auto ind = std::make_shared<CircularProgressIndicator>();
    ind->value = cfg.value;
    ind->value_color = accent_;
    return ind;
}

WidgetRef FluentDesignSystem::buildTooltip(const TooltipConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto txt = std::make_shared<Text>(cfg.message, tokens_.typography.body_small);
    auto pad = std::make_shared<Padding>();
    pad->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.sm);
    pad->child   = txt;
    BoxDecoration deco;
    deco.color = c.inverse_surface;
    deco.border_radius = tokens_.shape.radius_md;
    auto box = std::make_shared<DecoratedBox>();
    box->decoration = deco;
    box->child      = pad;
    return box;
}

WidgetRef FluentDesignSystem::buildListTile(const ListTileConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    if (cfg.leading) {
        row->children.push_back(cfg.leading);
        row->children.push_back(std::make_shared<SizedBox>(tokens_.spacing.md, 0.0f));
    }

    auto textCol = std::make_shared<Column>();
    textCol->cross_axis_alignment = CrossAxisAlignment::start;
    textCol->main_axis_size = MainAxisSize::min;
    if (cfg.title)    textCol->children.push_back(cfg.title);
    if (cfg.subtitle) textCol->children.push_back(cfg.subtitle);
    row->children.push_back(textCol);

    if (cfg.trailing) {
        row->children.push_back(std::make_shared<Expanded>());
        row->children.push_back(std::make_shared<SizedBox>(tokens_.spacing.md, 0.0f));
        row->children.push_back(cfg.trailing);
    }

    auto pad = std::make_shared<Padding>();
    pad->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.sm);
    pad->child   = row;

    auto detector = std::make_shared<GestureDetector>();
    detector->on_tap = (cfg.enabled && cfg.on_tap) ? cfg.on_tap : nullptr;
    detector->on_long_press = (cfg.enabled && cfg.on_long_press) ? cfg.on_long_press : nullptr;
    detector->child = pad;
    return detector;
}

WidgetRef FluentDesignSystem::buildDivider(const DividerConfig& cfg) const
{
    const auto& c = tokens_.colors;
    // Divider reads its color from Theme; use a direct Container for the
    // sketch so the Fluent outline_variant color is always applied.
    auto line = std::make_shared<Container>();
    line->height = 1.0f;
    line->color  = c.outline_variant;
    if (cfg.indent > 0.0f || cfg.end_indent > 0.0f) {
        auto pad = std::make_shared<Padding>();
        pad->padding = EdgeInsets(cfg.indent, 0.0f, cfg.end_indent, 0.0f);
        pad->child   = line;
        return pad;
    }
    return line;
}

// ----------------------------------------------------------------------------
// Remaining component builders — solid-fill sketches, not yet using real
// Mica/Acrylic materials (see FluentMaterial's doc comment).
// ----------------------------------------------------------------------------

WidgetRef FluentDesignSystem::buildAppBar(const AppBarConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> row_children;
    if (cfg.leading) {
        row_children.push_back(cfg.leading);
        row_children.push_back(SizedBox::from_width(tokens_.spacing.md));
    }
    if (cfg.title) {
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(cfg.title)));
    } else {
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(SizedBox::shrink())));
    }
    for (const auto& action : cfg.actions) {
        row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
        row_children.push_back(action);
    }

    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    row->children = std::move(row_children);

    auto padded = std::make_shared<Padding>();
    padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.md);
    padded->child   = row;

    // Fluent's CommandBar is flat — a surface fill with a hairline bottom
    // border, no elevation shadow (unlike Material's elevated app bar).
    // BoxBorder only supports a uniform all-sides border, so the hairline
    // is a separate 1px Container underneath — same technique
    // buildDivider() already uses for its own thin line.
    auto surface = std::make_shared<Container>();
    surface->color = c.surface;
    surface->child = padded;

    auto hairline = std::make_shared<Container>();
    hairline->height = 1.0f;
    hairline->color  = c.outline_variant;

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = {surface, hairline};
    return col;
}

WidgetRef FluentDesignSystem::buildNavigationBar(const NavigationBarConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> item_widgets;
    for (size_t i = 0; i < cfg.items.size(); ++i) {
        const auto& item = cfg.items[i];
        bool selected = static_cast<int>(i) == cfg.selected_index;

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::center;
        if (item.icon) col->children.push_back(item.icon);
        if (!item.label.empty()) {
            TextStyle ts;
            ts.font_size = 12.0f;
            ts.color = selected ? accent_ : c.on_surface_variant;
            col->children.push_back(std::make_shared<Text>(item.label, ts));
        }

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.sm, tokens_.spacing.xs);
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

    auto hairline = std::make_shared<Container>();
    hairline->height = 1.0f;
    hairline->color  = c.outline_variant;

    auto surface = std::make_shared<Container>();
    surface->color = c.surface;
    surface->child = row;

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = {hairline, surface};
    return col;
}

WidgetRef FluentDesignSystem::buildDialog(const DialogConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> children;

    if (cfg.title) {
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::only(24.0f, 24.0f, 24.0f, 0.0f);
        padded->child   = cfg.title;
        children.push_back(padded);
    }
    if (cfg.content) {
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::only(24.0f, 12.0f, 24.0f, 0.0f);
        padded->child   = cfg.content;
        children.push_back(padded);
    }
    if (!cfg.actions.empty()) {
        children.push_back(SizedBox::from_height(tokens_.spacing.xl));
        auto row = std::make_shared<Row>();
        row->main_axis_alignment = MainAxisAlignment::end;
        row->children = cfg.actions;
        auto action_pad = std::make_shared<Padding>();
        action_pad->padding = EdgeInsets::only(16.0f, 0.0f, 24.0f, 20.0f);
        action_pad->child   = row;
        children.push_back(action_pad);
    } else {
        children.push_back(SizedBox::from_height(tokens_.spacing.xl));
    }

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = std::move(children);

    auto dialog = std::make_shared<Dialog>();
    dialog->child            = col;
    dialog->background_color = c.surface;
    // Fluent 2's ContentDialog corner radius is 8dp — much less rounded
    // than MD3's 28dp "Extra Large" dialog shape.
    dialog->border_radius    = tokens_.shape.radius_lg;
    dialog->elevation        = tokens_.elevation.level3;
    dialog->max_width        = 320.0f; // Fluent's default ContentDialog width is narrower than MD3's.
    return dialog;
}

WidgetRef FluentDesignSystem::buildSnackBar(const SnackBarConfig& cfg) const
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
        action_style.color = accent_;
        auto action_text = std::make_shared<Text>(*cfg.action_label, action_style);

        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = cfg.on_action;
        gesture->child  = action_text;
        row_children.push_back(SizedBox::from_width(tokens_.spacing.lg));
        row_children.push_back(gesture);
    }

    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    row->children = std::move(row_children);

    auto sb = std::make_shared<SnackBar>();
    sb->content          = row;
    // Fluent's toast-like surfaces stay on the neutral surface color with a
    // border, unlike Material's inverse-color snackbar.
    sb->background_color = c.surface;
    sb->border_radius    = tokens_.shape.radius_md;
    sb->padding          = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.md);
    sb->duration_ms      = cfg.duration_ms;
    return sb;
}

WidgetRef FluentDesignSystem::buildPopupMenuButton(const PopupMenuConfig& cfg) const
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
    pmb->items         = std::move(items);
    pmb->on_selected   = cfg.on_selected;
    pmb->child         = cfg.child;
    pmb->popup_color   = c.surface;
    // Fluent 2 flyouts/overlays use an 8dp corner radius.
    pmb->border_radius = tokens_.shape.radius_lg;
    pmb->elevation     = tokens_.elevation.level2;
    return pmb;
}

WidgetRef FluentDesignSystem::buildDropdownButton(const DropdownConfig& cfg) const
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
    if (cfg.selected_value.has_value()) dd->value = *cfg.selected_value;
    if (cfg.on_changed) dd->on_changed = cfg.on_changed;
    dd->dropdown_color = c.surface;
    dd->border_radius  = tokens_.shape.radius_lg;
    dd->elevation      = tokens_.elevation.level2;
    return dd;
}

WidgetRef FluentDesignSystem::buildPrimaryActionButton(const PrimaryActionConfig& cfg) const
{
    // Treat as an accent-filled circular button for now.
    ButtonConfig bc;
    bc.label = cfg.label ? cfg.label : cfg.icon;
    bc.on_pressed = cfg.on_pressed;
    bc.enabled = cfg.enabled;
    bc.priority = ButtonPriority::primary;
    return buildButton(bc);
}

WidgetRef FluentDesignSystem::buildTabBar(const TabBarConfig& cfg) const
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
    tb->tabs                   = std::move(tabs);
    tb->indicator_color        = accent_;
    tb->label_color            = accent_;
    tb->unselected_label_color = c.on_surface_variant;
    // Fluent's Pivot underline is thinner than Material's 3dp indicator.
    tb->indicator_weight       = 2.0f;
    tb->background_color       = c.surface;
    return tb;
}

WidgetRef FluentDesignSystem::buildChip(const ChipConfig& cfg) const
{
    const auto& c = tokens_.colors;
    // Fluent 2 chips are neutral-surface pills that only pick up the accent
    // color when selected — unlike Material's tonal secondaryContainer
    // treatment, Fluent has no colored "container" role for this.
    Color bg     = cfg.selected ? withOpacity(accent_, 0.12f) : c.secondary_container;
    Color fg     = cfg.selected ? accent_ : c.on_surface_variant;
    Color border = cfg.selected ? accent_ : Color::fromARGB(0x00FFFFFF);

    auto tint_label = [&](WidgetRef label) -> WidgetRef {
        if (auto text = std::dynamic_pointer_cast<const Text>(label)) {
            TextStyle style = text->span.style;
            style.withColor(fg);
            return std::make_shared<Text>(text->span.text, style);
        }
        return label;
    };

    std::vector<WidgetRef> row_children;
    if (cfg.leading_icon) {
        row_children.push_back(cfg.leading_icon);
        row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
    }
    row_children.push_back(tint_label(cfg.label));
    if (cfg.on_deleted) {
        row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
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
    padded->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.xs);
    padded->child   = row;

    BoxDecoration deco;
    deco.color         = bg;
    deco.border_radius = tokens_.shape.radius_full; // Fluent chips are pills.
    deco.border         = BoxBorder::all(border, 1.0f);

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

WidgetRef FluentDesignSystem::buildSegmentedButton(const SegmentedConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto tint_label = [&](WidgetRef label, Color fg) -> WidgetRef {
        if (auto text = std::dynamic_pointer_cast<const Text>(label)) {
            TextStyle style = text->span.style;
            style.withColor(fg);
            return std::make_shared<Text>(text->span.text, style);
        }
        return label;
    };

    std::vector<WidgetRef> row_children;
    for (size_t i = 0; i < cfg.segments.size(); ++i) {
        const auto& seg = cfg.segments[i];
        bool selected = static_cast<int>(i) == cfg.selected_index;
        bool is_last  = i + 1 == cfg.segments.size();
        // Fluent's selected-segment treatment is an accent tint, not
        // Material's secondaryContainer tonal fill — same convention as
        // buildChip()/buildToggleButtons() elsewhere in this file.
        const Color fg = selected ? accent_ : c.on_surface;

        std::vector<WidgetRef> content_children;
        if (seg.icon) content_children.push_back(seg.icon);
        if (seg.label) content_children.push_back(tint_label(seg.label, fg));
        auto content_row = std::make_shared<Row>();
        content_row->main_axis_alignment = MainAxisAlignment::center;
        content_row->main_axis_size = MainAxisSize::min;
        content_row->children = std::move(content_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.sm);
        padded->child   = content_row;

        BoxDecoration deco;
        deco.color = selected ? withOpacity(accent_, 0.12f) : Color::fromARGB(0x00FFFFFF);
        if (!is_last) deco.border = BoxBorder::all(c.outline, 1.0f);
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

    BoxDecoration outer;
    // Fluent's SegmentedControl uses its normal 4dp control radius, not
    // Material's fully-rounded stadium shape.
    outer.border_radius = tokens_.shape.radius_md;
    outer.border        = BoxBorder::all(c.outline, 1.0f);
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = outer;
    decorated->child      = container_row;

    auto sized = std::make_shared<SizedBox>();
    sized->height = 36.0f;
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

WidgetRef FluentDesignSystem::buildBottomSheet(const BottomSheetConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> children;

    if (cfg.show_drag_handle) {
        BoxDecoration handle_deco;
        handle_deco.color         = c.outline_variant;
        handle_deco.border_radius = tokens_.shape.radius_full;
        auto handle_decorated = std::make_shared<DecoratedBox>();
        handle_decorated->decoration = handle_deco;
        auto sized_handle = std::make_shared<SizedBox>();
        sized_handle->width  = 32.0f;
        sized_handle->height = 4.0f;
        sized_handle->child  = handle_decorated;

        // Align with no height_factor set expands to fill whatever height
        // it's given (correct, intentional Align/Center behavior on its
        // own) — but as a bare Column child under Column's loose main-axis
        // constraint, that "fill available height" size gets summed into
        // the Column's own MainAxisSize::min total, silently inflating the
        // whole sheet to fill the canvas instead of hugging its actual
        // content. Bounding it back down to the handle's real height (this
        // is what actually broke bottomSheet_partial's fidelity capture —
        // confirmed by tracing RenderAlign/RenderFlex's layout math).
        auto centered_handle = SizedBox::from_height(4.0f,
            std::make_shared<Align>(Alignment::center(), sized_handle));

        auto padded_handle = std::make_shared<Padding>();
        padded_handle->padding = EdgeInsets::symmetric(0.0f, tokens_.spacing.md);
        padded_handle->child   = centered_handle;
        children.push_back(padded_handle);
    }
    children.push_back(cfg.child);

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = std::move(children);

    // Fluent overlays use an 8dp corner radius, same as buildPopupMenuButton/
    // buildDropdownButton's flyout radius, with a subtle border rather than
    // Material's pure-shadow elevation.
    BoxDecoration deco;
    deco.color         = c.surface;
    deco.border        = BoxBorder::all(c.outline_variant, 1.0f);
    deco.border_radius = tokens_.shape.radius_lg;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = deco;
    decorated->child      = col;
    return decorated;
}

WidgetRef FluentDesignSystem::buildBadge(const BadgeConfig& cfg) const
{
    const auto& c = tokens_.colors;

    // Fluent's InfoBadge default ("Attention") style is accent-colored,
    // unlike Material's badge which is always the error/red role.
    WidgetRef badge_content;
    float diameter = 8.0f; // Fluent's dot InfoBadge is slightly larger than MD3's 6dp.
    if (cfg.label.has_value()) {
        diameter = 16.0f;
        TextStyle ts;
        ts.font_size = 11.0f;
        ts.color = c.on_primary;
        badge_content = std::make_shared<Text>(*cfg.label, ts);
    }

    BoxDecoration deco;
    deco.color         = accent_;
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
    positioned->right = -2.0f;
    positioned->top   = -2.0f;
    positioned->child = sized_badge;

    auto stack = std::make_shared<Stack>();
    stack->children = {cfg.child, positioned};
    return stack;
}

WidgetRef FluentDesignSystem::buildIconButton(const IconButtonConfig& cfg) const
{
    const auto& c = tokens_.colors;
    auto padded = std::make_shared<Padding>();
    padded->padding = EdgeInsets::all(tokens_.spacing.sm);
    padded->child   = cfg.icon;

    BoxDecoration deco;
    // Fluent icon buttons stay neutral even when "filled" (a subtle
    // surface-variant tint), unlike Material's solid primary fill —
    // Fluent reserves the accent color for the *selected/checked* state.
    deco.color         = cfg.selected ? withOpacity(accent_, 0.15f) : Color::fromARGB(0x00FFFFFF);
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

WidgetRef FluentDesignSystem::buildStepper(const StepperConfig& cfg) const
{
    const auto& c = tokens_.colors;

    // Fluent's NumberBox is a single outlined field with inline spin
    // buttons, not three separate tonal buttons like Material's — modeled
    // here as one bordered box containing -/value/+.
    auto makeGlyph = [&](const std::string& glyph, std::function<void()> on_tap) {
        auto text_widget = std::make_shared<Text>(glyph,
            TextStyle{}.withFontSize(14.0f).withColor(c.on_surface));
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.sm, tokens_.spacing.xs);
        padded->child   = text_widget;
        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = std::move(on_tap);
        gesture->child  = padded;
        return gesture;
    };

    const bool can_dec = cfg.enabled && cfg.on_changed && cfg.value > cfg.min;
    const bool can_inc = cfg.enabled && cfg.on_changed && cfg.value < cfg.max;

    auto dec = makeGlyph("-", can_dec
        ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, lo = cfg.min] {
              cb(std::max(lo, v - step));
          })
        : nullptr);
    auto inc = makeGlyph("+", can_inc
        ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, hi = cfg.max] {
              cb(std::min(hi, v + step));
          })
        : nullptr);

    auto value_text = std::make_shared<Text>(std::to_string(cfg.value),
        TextStyle{}.withFontSize(14.0f).withColor(c.on_surface));
    auto padded_value = std::make_shared<Padding>();
    padded_value->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.xs);
    padded_value->child   = value_text;

    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    row->main_axis_size = MainAxisSize::min;
    row->children = {dec, padded_value, inc};

    BoxDecoration deco;
    deco.color         = c.surface;
    deco.border        = BoxBorder::all(c.outline, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = deco;
    decorated->child      = row;

    if (!cfg.enabled) {
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = decorated;
        return faded;
    }
    return decorated;
}

WidgetRef FluentDesignSystem::buildRatingIndicator(const RatingConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> stars;
    for (int i = 0; i < cfg.max; ++i) {
        bool filled = i < cfg.value;
        auto glyph = std::make_shared<Text>(filled ? "*" : "-",
            TextStyle{}.withFontSize(18.0f).withColor(filled ? accent_ : c.outline));
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

WidgetRef FluentDesignSystem::buildActionSheet(const ActionSheetConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> children;

    if (cfg.title) {
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::only(tokens_.spacing.lg, tokens_.spacing.lg, tokens_.spacing.lg, tokens_.spacing.sm);
        padded->child   = cfg.title;
        children.push_back(padded);
    }

    auto addRow = [&](const std::string& label, std::function<void()> on_tap, Color color) {
        TextStyle ts;
        ts.font_size = 14.0f;
        ts.color = color;
        auto text = std::make_shared<Text>(label, ts);
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.md);
        padded->child   = text;
        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = std::move(on_tap);
        gesture->child  = padded;
        children.push_back(gesture);
    };

    for (const auto& action : cfg.actions)
        addRow(action.label, action.on_selected, action.destructive ? c.error : c.on_surface);
    if (cfg.on_cancel)
        addRow("Cancel", cfg.on_cancel, c.on_surface);

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = std::move(children);

    // Same flat-surface + hairline-border overlay treatment as
    // buildBottomSheet() — Fluent has no separate "action sheet" visual
    // language distinct from its other flyout/overlay surfaces.
    BoxDecoration deco;
    deco.color         = c.surface;
    deco.border        = BoxBorder::all(c.outline_variant, 1.0f);
    deco.border_radius = tokens_.shape.radius_lg;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = deco;
    decorated->child      = col;
    return decorated;
}

WidgetRef FluentDesignSystem::buildSearchField(const SearchFieldConfig& cfg) const
{
    const auto& c = tokens_.colors;

    // Fluent's AutoSuggestBox is an outlined box (border + radius_md),
    // unlike Material's filled pill — matches buildTextField()'s existing
    // outline convention.
    auto icon_text = std::make_shared<Text>("*",
        TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));

    auto controller = std::make_shared<TextEditingController>(cfg.value);
    auto tf = std::make_shared<TextField>(controller, cfg.placeholder);
    tf->fill_color        = Color::fromARGB(0x00FFFFFF);
    tf->border_color       = Color::fromARGB(0x00FFFFFF);
    tf->placeholder_color = c.on_surface_variant;
    tf->cursor_color      = accent_;
    if (cfg.on_changed)
        tf->on_changed = [cb = cfg.on_changed](const std::string& v) { cb(v); };

    std::vector<WidgetRef> row_children;
    row_children.push_back(icon_text);
    row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
    row_children.push_back(std::make_shared<Expanded>(WidgetRef(tf)));
    if (!cfg.value.empty() && cfg.on_clear) {
        auto clear = std::make_shared<Text>("x",
            TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));
        auto gesture = std::make_shared<GestureDetector>();
        gesture->on_tap = cfg.on_clear;
        gesture->child  = clear;
        row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
        row_children.push_back(gesture);
    }

    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    row->children = std::move(row_children);

    auto padded = std::make_shared<Padding>();
    padded->padding = EdgeInsets::symmetric(tokens_.spacing.md, tokens_.spacing.sm);
    padded->child   = row;

    BoxDecoration deco;
    deco.color         = c.surface;
    deco.border        = BoxBorder::all(c.outline, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;
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

namespace
{
    WidgetRef buildFluentTriggerField(const DesignTokens& tokens, const std::string& glyph,
                                       const std::string& label, std::function<void()> on_tap,
                                       bool enabled)
    {
        const auto& c = tokens.colors;
        auto icon_text = std::make_shared<Text>(glyph,
            TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));
        auto label_text = std::make_shared<Text>(label,
            TextStyle{}.withFontSize(14.0f).withColor(c.on_surface));

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = {
            std::make_shared<Expanded>(WidgetRef(label_text)),
            SizedBox::from_width(tokens.spacing.sm),
            icon_text,
        };

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens.spacing.md, tokens.spacing.sm);
        padded->child   = row;

        BoxDecoration deco;
        deco.border        = BoxBorder::all(c.outline, 1.0f);
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

WidgetRef FluentDesignSystem::buildDatePicker(const DatePickerConfig& cfg) const
{
    return buildFluentTriggerField(tokens_, "date", cfg.label, cfg.on_tap, cfg.enabled);
}

WidgetRef FluentDesignSystem::buildTimePicker(const TimePickerConfig& cfg) const
{
    return buildFluentTriggerField(tokens_, "time", cfg.label, cfg.on_tap, cfg.enabled);
}

WidgetRef FluentDesignSystem::buildExpansionTile(const ExpansionTileConfig& cfg) const
{
    const auto& c = tokens_.colors;

    std::vector<WidgetRef> header_children;
    if (cfg.leading) {
        header_children.push_back(cfg.leading);
        header_children.push_back(SizedBox::from_width(tokens_.spacing.lg));
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
        TextStyle{}.withFontSize(13.0f).withColor(c.on_surface_variant)));

    auto header_row = std::make_shared<Row>();
    header_row->cross_axis_alignment = CrossAxisAlignment::center;
    header_row->children = std::move(header_children);

    auto header_padded = std::make_shared<Padding>();
    header_padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.md);
    header_padded->child   = header_row;

    auto header_gesture = std::make_shared<GestureDetector>();
    if (cfg.enabled && cfg.on_expansion_changed) {
        header_gesture->on_tap = [cb = cfg.on_expansion_changed, expanded = cfg.expanded] { cb(!expanded); };
    }
    header_gesture->child = header_padded;

    std::vector<WidgetRef> children = {header_gesture};
    if (cfg.expanded && cfg.children_content) {
        // Real Fluent 2 Expander draws a hairline divider between the
        // header and its content, and indents the content to match the
        // header's own padding — matches the reference WinUI3 Expander
        // control (ground truth), unlike a bare unpadded child.
        auto divider = std::make_shared<Container>();
        divider->height = 1.0f;
        divider->color  = c.outline_variant;
        children.push_back(divider);

        auto content_padded = std::make_shared<Padding>();
        content_padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.xxl);
        content_padded->child   = cfg.children_content;

        // Real Fluent 2 Expander fills its content region with a distinct,
        // slightly darker tone than the header — matches the reference
        // WinUI3 Expander's two-tone header/content look, unlike a single
        // flat surface color across the whole card.
        auto content_bg = std::make_shared<Container>();
        content_bg->color = c.background;
        content_bg->child = content_padded;
        children.push_back(content_bg);
    }

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = std::move(children);

    // Fluent's Expander is bordered and flat, unlike Material's borderless
    // tile — matches buildTextField()/buildSearchField()'s outline
    // convention throughout this file.
    BoxDecoration deco;
    deco.color         = c.surface;
    deco.border        = BoxBorder::all(c.outline_variant, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = deco;
    decorated->child      = col;

    if (!cfg.enabled) {
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = decorated;
        return faded;
    }
    return decorated;
}

WidgetRef FluentDesignSystem::buildToggleButtons(const ToggleButtonsConfig& cfg) const
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
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.sm);
        padded->child   = content_row;

        // Fluent's selected-segment treatment is an accent tint, not a
        // Material-style tonal secondaryContainer fill.
        BoxDecoration deco;
        deco.color  = item.selected ? withOpacity(accent_, 0.12f) : Color::fromARGB(0x00FFFFFF);
        deco.border = BoxBorder::all(c.outline, 1.0f);
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

WidgetRef FluentDesignSystem::buildBanner(const BannerConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> row_children;
    if (cfg.leading) {
        row_children.push_back(cfg.leading);
        row_children.push_back(SizedBox::from_width(tokens_.spacing.lg));
    }
    row_children.push_back(std::make_shared<Expanded>(WidgetRef(cfg.content)));
    for (const auto& action : cfg.actions) {
        row_children.push_back(SizedBox::from_width(tokens_.spacing.sm));
        row_children.push_back(action);
    }

    auto row = std::make_shared<Row>();
    row->cross_axis_alignment = CrossAxisAlignment::center;
    row->children = std::move(row_children);

    auto padded = std::make_shared<Padding>();
    padded->padding = EdgeInsets::all(tokens_.spacing.lg);
    padded->child   = row;

    // Fluent's InfoBar is a self-contained rounded card (informational
    // tint + border), not a full-width strip with a bottom divider like
    // Material's banner.
    BoxDecoration deco;
    deco.color         = c.secondary_container;
    deco.border        = BoxBorder::all(c.outline_variant, 1.0f);
    deco.border_radius = tokens_.shape.radius_md;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = deco;
    decorated->child      = padded;
    return decorated;
}

WidgetRef FluentDesignSystem::buildNavigationRail(const NavigationRailConfig& cfg) const
{
    const auto& c = tokens_.colors;
    std::vector<WidgetRef> item_widgets;
    for (size_t i = 0; i < cfg.items.size(); ++i) {
        const auto& item = cfg.items[i];
        bool selected = static_cast<int>(i) == cfg.selected_index;

        WidgetRef icon_widget = item.icon;
        if (icon_widget && selected) {
            // Fluent's selected-rail-item indicator is an accent tint, not
            // Material's secondaryContainer tonal pill.
            BoxDecoration pill;
            pill.color         = withOpacity(accent_, 0.15f);
            pill.border_radius = tokens_.shape.radius_full;
            auto indicator = std::make_shared<DecoratedBox>();
            indicator->decoration = pill;
            auto padded_icon = std::make_shared<Padding>();
            padded_icon->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.sm);
            padded_icon->child   = icon_widget;
            indicator->child = padded_icon;
            icon_widget = indicator;
        }

        std::vector<WidgetRef> content;
        if (icon_widget) content.push_back(icon_widget);
        if (cfg.extended && !item.label.empty()) {
            content.push_back(SizedBox::from_width(tokens_.spacing.md));
            TextStyle ts;
            ts.font_size = 13.0f;
            ts.color = selected ? accent_ : c.on_surface;
            content.push_back(std::make_shared<Text>(item.label, ts));
        }

        WidgetRef entry_content;
        if (cfg.extended) {
            auto row = std::make_shared<Row>();
            row->main_axis_alignment = MainAxisAlignment::start;
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->children = std::move(content);
            entry_content = row;
        } else {
            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::center;
            col->children = std::move(content);
            entry_content = col;
        }

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(cfg.extended ? tokens_.spacing.lg : tokens_.spacing.sm, tokens_.spacing.sm);
        padded->child   = entry_content;

        WidgetRef entry = padded;
        if (cfg.on_tap) {
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = [cb = cfg.on_tap, idx = static_cast<int>(i)] { cb(idx); };
            gesture->child  = padded;
            entry = gesture;
        }
        item_widgets.push_back(entry);
        if (i + 1 < cfg.items.size()) item_widgets.push_back(SizedBox::from_height(tokens_.spacing.md));
    }

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = cfg.extended ? CrossAxisAlignment::stretch : CrossAxisAlignment::center;
    col->children = std::move(item_widgets);

    auto padded_col = std::make_shared<Padding>();
    padded_col->padding = EdgeInsets::symmetric(0.0f, tokens_.spacing.lg);
    padded_col->child   = col;

    BoxDecoration outer;
    outer.color = c.surface;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = outer;
    decorated->child      = padded_col;
    return decorated;
}

WidgetRef FluentDesignSystem::buildDataTable(const DataTableConfig& cfg) const
{
    const auto& c = tokens_.colors;

    auto makeCell = [&](WidgetRef content) {
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(tokens_.spacing.lg, tokens_.spacing.md);
        padded->child   = std::move(content);
        return std::make_shared<Expanded>(WidgetRef(padded));
    };

    std::vector<WidgetRef> header_cells;
    for (const auto& col_name : cfg.columns) {
        TextStyle ts;
        ts.font_size = 13.0f;
        ts.font_weight = FontWeight::bold;
        ts.color = c.on_surface_variant;
        header_cells.push_back(makeCell(std::make_shared<Text>(col_name, ts)));
    }
    auto header_row = std::make_shared<Row>();
    header_row->children = std::move(header_cells);

    std::vector<WidgetRef> table_children = {header_row};
    auto header_divider = std::make_shared<Container>();
    header_divider->height = 1.0f;
    header_divider->color  = c.outline;
    table_children.push_back(header_divider);

    for (const auto& data_row : cfg.rows) {
        std::vector<WidgetRef> cells;
        for (const auto& cell : data_row) cells.push_back(makeCell(cell));
        auto row = std::make_shared<Row>();
        row->children = std::move(cells);
        table_children.push_back(row);

        auto divider = std::make_shared<Container>();
        divider->height = 1.0f;
        divider->color  = c.outline_variant;
        table_children.push_back(divider);
    }

    auto col = std::make_shared<Column>();
    col->main_axis_size = MainAxisSize::min;
    col->cross_axis_alignment = CrossAxisAlignment::stretch;
    col->children = std::move(table_children);

    BoxDecoration outer;
    outer.color         = c.surface;
    outer.border        = BoxBorder::all(c.outline_variant, 1.0f);
    outer.border_radius = tokens_.shape.radius_md;
    auto decorated = std::make_shared<DecoratedBox>();
    decorated->decoration = outer;
    decorated->child      = col;
    return decorated;
}

} // namespace systems::leal::campello_widgets
