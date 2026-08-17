#include <campello_fluent/fluent_design_system.hpp>

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
Color fluentDarkSurface()  { return Color::fromARGB(0xFF1C1C1C); }

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

// Fallback placeholder for builders that are not yet implemented.
WidgetRef placeholder(const std::string& label, const Color& bg, const Color& fg)
{
    auto txt = std::make_shared<Text>(label, TextStyle{}.withFontSize(14.0f).withColor(fg));
    auto pad = std::make_shared<Padding>();
    pad->padding = EdgeInsets::all(16.0f);
    pad->child   = txt;
    auto box = std::make_shared<DecoratedBox>();
    box->decoration.color = bg;
    box->child = pad;
    return box;
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

    auto detector = std::make_shared<GestureDetector>();
    detector->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
    detector->child  = box;

    if (!cfg.enabled || !cfg.on_pressed) {
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = detector;
        return faded;
    }
    return detector;
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

    // Fluent cards: 8dp rounded corners, 1dp outline, no shadow by default.
    BoxDecoration deco;
    deco.color  = c.surface;
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
// Placeholder builders: stubbed with a labeled rectangle for now.
// ----------------------------------------------------------------------------

WidgetRef FluentDesignSystem::buildAppBar(const AppBarConfig&) const
{
    return placeholder("Fluent AppBar (CommandBar)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildNavigationBar(const NavigationBarConfig&) const
{
    return placeholder("Fluent NavigationBar", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildDialog(const DialogConfig&) const
{
    return placeholder("Fluent Dialog (ContentDialog)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildSnackBar(const SnackBarConfig&) const
{
    return placeholder("Fluent SnackBar", tokens_.colors.inverse_surface, tokens_.colors.inverse_on_surface);
}

WidgetRef FluentDesignSystem::buildPopupMenuButton(const PopupMenuConfig&) const
{
    return placeholder("Fluent PopupMenuButton", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildDropdownButton(const DropdownConfig&) const
{
    return placeholder("Fluent DropdownButton", tokens_.colors.surface, tokens_.colors.on_surface);
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

WidgetRef FluentDesignSystem::buildTabBar(const TabBarConfig&) const
{
    return placeholder("Fluent TabBar (Pivot)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildChip(const ChipConfig&) const
{
    return placeholder("Fluent Chip", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildSegmentedButton(const SegmentedConfig&) const
{
    return placeholder("Fluent SegmentedButton", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildBottomSheet(const BottomSheetConfig&) const
{
    return placeholder("Fluent BottomSheet", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildBadge(const BadgeConfig&) const
{
    return placeholder("Fluent Badge", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildIconButton(const IconButtonConfig&) const
{
    return placeholder("Fluent IconButton", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildStepper(const StepperConfig&) const
{
    return placeholder("Fluent Stepper (NumberBox)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildRatingIndicator(const RatingConfig&) const
{
    return placeholder("Fluent RatingIndicator", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildActionSheet(const ActionSheetConfig&) const
{
    return placeholder("Fluent ActionSheet", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildSearchField(const SearchFieldConfig&) const
{
    return placeholder("Fluent SearchField", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildDatePicker(const DatePickerConfig&) const
{
    return placeholder("Fluent DatePicker", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildTimePicker(const TimePickerConfig&) const
{
    return placeholder("Fluent TimePicker", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildExpansionTile(const ExpansionTileConfig&) const
{
    return placeholder("Fluent ExpansionTile", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildToggleButtons(const ToggleButtonsConfig&) const
{
    return placeholder("Fluent ToggleButtons", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildBanner(const BannerConfig&) const
{
    return placeholder("Fluent Banner (InfoBar)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildNavigationRail(const NavigationRailConfig&) const
{
    return placeholder("Fluent NavigationRail (NavigationView)", tokens_.colors.surface, tokens_.colors.on_surface);
}

WidgetRef FluentDesignSystem::buildDataTable(const DataTableConfig&) const
{
    return placeholder("Fluent DataTable", tokens_.colors.surface, tokens_.colors.on_surface);
}

} // namespace systems::leal::campello_widgets
