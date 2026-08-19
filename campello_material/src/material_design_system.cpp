#include <campello_material/material_design_system.hpp>

// Widgets
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/dropdown_button.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/icon.hpp>
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

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    Color MaterialDesignSystem::withOpacity(Color c, float opacity)
    {
        return Color::fromRGBA(c.r, c.g, c.b, c.a * opacity);
    }

    namespace
    {
        // MD3 type scale — sizes match the published spec exactly; "Medium"
        // weight roles (title-medium/-small, label-*) map to our bold, since
        // TextStyle only distinguishes normal/bold.
        TypographyScale makeTypography(Color on_surface)
        {
            TypographyScale t;
            t.display_large   = TextStyle{}.withFontSize(57.0f).withColor(on_surface);
            t.display_medium  = TextStyle{}.withFontSize(45.0f).withColor(on_surface);
            t.display_small   = TextStyle{}.withFontSize(36.0f).withColor(on_surface);
            t.headline_large  = TextStyle{}.withFontSize(32.0f).withColor(on_surface);
            t.headline_medium = TextStyle{}.withFontSize(28.0f).withColor(on_surface);
            t.headline_small  = TextStyle{}.withFontSize(24.0f).withColor(on_surface);
            t.title_large     = TextStyle{}.withFontSize(22.0f).withColor(on_surface);
            t.title_medium    = TextStyle{}.withFontSize(16.0f).bold().withColor(on_surface);
            t.title_small     = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
            t.body_large      = TextStyle{}.withFontSize(16.0f).withColor(on_surface);
            t.body_medium     = TextStyle{}.withFontSize(14.0f).withColor(on_surface);
            t.body_small      = TextStyle{}.withFontSize(12.0f).withColor(on_surface);
            t.label_large     = TextStyle{}.withFontSize(14.0f).bold().withColor(on_surface);
            t.label_medium    = TextStyle{}.withFontSize(12.0f).bold().withColor(on_surface);
            t.label_small     = TextStyle{}.withFontSize(11.0f).bold().withColor(on_surface);
            return t;
        }

        // MD3 baseline tonal palette, seed color #6750A4.
        DesignTokens makeLightTokens()
        {
            DesignTokens t;
            t.brightness = Brightness::light;

            t.colors.primary            = Color::fromARGB(0xFF6750A4);
            t.colors.on_primary         = Color::fromARGB(0xFFFFFFFF);
            t.colors.primary_container    = Color::fromARGB(0xFFEADDFF);
            t.colors.on_primary_container = Color::fromARGB(0xFF21005D);
            t.colors.secondary          = Color::fromARGB(0xFF625B71);
            t.colors.on_secondary       = Color::fromARGB(0xFFFFFFFF);
            t.colors.secondary_container    = Color::fromARGB(0xFFE8DEF8);
            t.colors.on_secondary_container = Color::fromARGB(0xFF1D192B);
            t.colors.tertiary           = Color::fromARGB(0xFF7D5260);
            t.colors.on_tertiary        = Color::fromARGB(0xFFFFFFFF);
            t.colors.tertiary_container    = Color::fromARGB(0xFFFFD8E4);
            t.colors.on_tertiary_container = Color::fromARGB(0xFF31111D);
            t.colors.surface            = Color::fromARGB(0xFFFFFBFE);
            t.colors.on_surface         = Color::fromARGB(0xFF1C1B1F);
            t.colors.surface_variant    = Color::fromARGB(0xFFE7E0EC);
            t.colors.on_surface_variant = Color::fromARGB(0xFF49454F);
            t.colors.background         = Color::fromARGB(0xFFFFFBFE);
            t.colors.on_background      = Color::fromARGB(0xFF1C1B1F);
            t.colors.error              = Color::fromARGB(0xFFB3261E);
            t.colors.on_error           = Color::fromARGB(0xFFFFFFFF);
            t.colors.error_container    = Color::fromARGB(0xFFF9DEDC);
            t.colors.on_error_container = Color::fromARGB(0xFF410E0B);
            t.colors.success            = Color::fromARGB(0xFF146C2E);
            t.colors.on_success         = Color::fromARGB(0xFFFFFFFF);
            t.colors.warning            = Color::fromARGB(0xFF7A5900);
            t.colors.on_warning         = Color::fromARGB(0xFFFFFFFF);
            t.colors.outline            = Color::fromARGB(0xFF79747E);
            t.colors.outline_variant    = Color::fromARGB(0xFFCAC4D0);
            t.colors.shadow             = Color::fromARGB(0xFF000000);
            t.colors.scrim              = Color::fromARGB(0x80000000);
            t.colors.inverse_surface    = Color::fromARGB(0xFF313033);
            t.colors.inverse_on_surface = Color::fromARGB(0xFFF4EFF4);
            t.colors.inverse_primary    = Color::fromARGB(0xFFD0BCFF);

            // MD3 shape scale: extra-small/small/medium/large/extra-large.
            t.shape.radius_xs = 4.0f;
            t.shape.radius_sm = 8.0f;
            t.shape.radius_md = 12.0f;
            t.shape.radius_lg = 16.0f;
            t.shape.radius_xl = 28.0f;
            // elevation levels (0/1/3/6/8/12dp) already match the
            // ElevationTokens default — no override needed.

            t.typography = makeTypography(t.colors.on_surface);
            return t;
        }

        DesignTokens makeDarkTokens()
        {
            DesignTokens t;
            t.brightness = Brightness::dark;

            t.colors.primary            = Color::fromARGB(0xFFD0BCFF);
            t.colors.on_primary         = Color::fromARGB(0xFF381E72);
            t.colors.primary_container    = Color::fromARGB(0xFF4F378B);
            t.colors.on_primary_container = Color::fromARGB(0xFFEADDFF);
            t.colors.secondary          = Color::fromARGB(0xFFCCC2DC);
            t.colors.on_secondary       = Color::fromARGB(0xFF332D41);
            t.colors.secondary_container    = Color::fromARGB(0xFF4A4458);
            t.colors.on_secondary_container = Color::fromARGB(0xFFE8DEF8);
            t.colors.tertiary           = Color::fromARGB(0xFFEFB8C8);
            t.colors.on_tertiary        = Color::fromARGB(0xFF492532);
            t.colors.tertiary_container    = Color::fromARGB(0xFF633B48);
            t.colors.on_tertiary_container = Color::fromARGB(0xFFFFD8E4);
            // 0x141218, not the 0x1C1B1F commonly cited as the MD3 "spec"
            // dark surface tone — confirmed against a real Android capture
            // of Compose Material3's actual darkColorScheme() default
            // (sampled background pixel: (20,18,24) exactly). The two
            // hex values are a well-known small discrepancy between
            // hand-copied spec-sheet tones and the real HCT-generated
            // Compose defaults.
            t.colors.surface            = Color::fromARGB(0xFF141218);
            t.colors.on_surface         = Color::fromARGB(0xFFE6E1E5);
            t.colors.surface_variant    = Color::fromARGB(0xFF49454F);
            t.colors.on_surface_variant = Color::fromARGB(0xFFCAC4D0);
            t.colors.background         = Color::fromARGB(0xFF141218);
            t.colors.on_background      = Color::fromARGB(0xFFE6E1E5);
            t.colors.error              = Color::fromARGB(0xFFF2B8B5);
            t.colors.on_error           = Color::fromARGB(0xFF601410);
            t.colors.error_container    = Color::fromARGB(0xFF8C1D18);
            t.colors.on_error_container = Color::fromARGB(0xFFF9DEDC);
            t.colors.success            = Color::fromARGB(0xFF7DDB8D);
            t.colors.on_success         = Color::fromARGB(0xFF0F3D1C);
            t.colors.warning            = Color::fromARGB(0xFFF0C34C);
            t.colors.on_warning         = Color::fromARGB(0xFF3F2E00);
            t.colors.outline            = Color::fromARGB(0xFF938F99);
            t.colors.outline_variant    = Color::fromARGB(0xFF49454F);
            t.colors.shadow             = Color::fromARGB(0xFF000000);
            t.colors.scrim              = Color::fromARGB(0x80000000);
            t.colors.inverse_surface    = Color::fromARGB(0xFFE6E1E5);
            t.colors.inverse_on_surface = Color::fromARGB(0xFF313033);
            t.colors.inverse_primary    = Color::fromARGB(0xFF6750A4);

            t.shape.radius_xs = 4.0f;
            t.shape.radius_sm = 8.0f;
            t.shape.radius_md = 12.0f;
            t.shape.radius_lg = 16.0f;
            t.shape.radius_xl = 28.0f;

            t.typography = makeTypography(t.colors.on_surface);
            return t;
        }
        // M3 Expressive Phase A: same palette/type ramp as baseline MD3 (see
        // header doc), rounder shape scale within the existing 7 fields.
        DesignTokens makeExpressiveLightTokens()
        {
            DesignTokens t = makeLightTokens();
            t.shape.radius_lg = 20.0f;
            t.shape.radius_xl = 32.0f;
            return t;
        }

        DesignTokens makeExpressiveDarkTokens()
        {
            DesignTokens t = makeDarkTokens();
            t.shape.radius_lg = 20.0f;
            t.shape.radius_xl = 32.0f;
            return t;
        }
    } // namespace

    MaterialDesignSystem::MaterialDesignSystem() : tokens_(makeLightTokens()) {}
    MaterialDesignSystem::MaterialDesignSystem(DesignTokens tokens) : tokens_(std::move(tokens)) {}

    MaterialDesignSystem MaterialDesignSystem::light() { return MaterialDesignSystem(makeLightTokens()); }
    MaterialDesignSystem MaterialDesignSystem::dark()  { return MaterialDesignSystem(makeDarkTokens()); }
    MaterialDesignSystem MaterialDesignSystem::expressiveLight() { return MaterialDesignSystem(makeExpressiveLightTokens()); }
    MaterialDesignSystem MaterialDesignSystem::expressiveDark()  { return MaterialDesignSystem(makeExpressiveDarkTokens()); }

    // -----------------------------------------------------------------------
    // Button — MD3 Filled Button: fully rounded (stadium), flat (0dp elevation)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildButton(const ButtonConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        Color bg, fg;
        bool outlined = false;

        switch (cfg.priority) {
            case ButtonPriority::secondary:
                bg = c.secondary;
                fg = c.on_secondary;
                break;
            case ButtonPriority::tertiary:
                bg = c.surface;
                fg = c.on_surface;
                outlined = true;
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

        // The label is caller-supplied plain text (see themed_component_
        // harness.cpp's `cfg.label = text("Button")`, default TextStyle —
        // black) with no expectation that it already carries the right
        // on-color; buildButton() owns applying `fg`, the same way the
        // background color is owned here rather than by the caller. Never
        // discarding `fg` unapplied was a real, previously-uncaught bug —
        // first surfaced by real-device (Android M3 Expressive) fidelity
        // testing, since this builder had no prior visual verification.
        auto tint_label = [&](WidgetRef label) -> WidgetRef {
            if (auto text = std::dynamic_pointer_cast<const Text>(label)) {
                TextStyle style = text->span.style;
                style.withColor(fg);
                return std::make_shared<Text>(text->span.text, style);
            }
            return label;
        };

        WidgetRef content = tint_label(cfg.label);
        if (cfg.leading_icon || cfg.trailing_icon) {
            auto row = std::make_shared<Row>();
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            if (cfg.leading_icon) row->children.push_back(cfg.leading_icon);
            row->children.push_back(tint_label(cfg.label));
            if (cfg.trailing_icon) row->children.push_back(cfg.trailing_icon);
            content = row;
        }

        auto padded = std::make_shared<Padding>();
        // EdgeInsets::symmetric(vertical, horizontal) — confirmed via real
        // Android M3 Expressive capture comparison that this call previously
        // had the arguments swapped (24pt applied vertically instead of
        // horizontally), producing a near-square/circular button instead of
        // MD3's wide stadium pill. The same swapped-argument mistake likely
        // recurs at this file's other EdgeInsets::symmetric() call sites —
        // unconfirmed without a real capture per builder, so left alone
        // pending evidence rather than blindly "fixed" here.
        padded->padding = EdgeInsets::symmetric(10.0f, 24.0f); // MD3 filled-button padding
        padded->child   = content;

        // Real M3 OutlinedButton is transparent (border only) — unlike
        // filled/tonal buttons, it never paints its own container color.
        // Confirmed against a real capture: the "tertiary"/outlined
        // button's box was fully transparent, not filled with `bg`
        // (c.surface) as this unconditionally did before.
        BoxDecoration deco;
        if (!outlined) deco.color = bg;
        deco.border_radius = tokens_.shape.radius_full; // stadium shape
        if (outlined) deco.border = BoxBorder::all(c.outline, 1.0f);

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        auto detector = std::make_shared<GestureDetector>();
        detector->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
        detector->child  = decorated;

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

    WidgetRef MaterialDesignSystem::buildSwitch(const SwitchConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sw = std::make_shared<Switch>();
        sw->value = cfg.value;
        sw->active_track_color   = c.primary;
        sw->inactive_track_color = c.surface_variant;
        sw->active_thumb_color   = c.on_primary;
        sw->inactive_thumb_color = c.outline;

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

    // -----------------------------------------------------------------------
    // Checkbox
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildCheckbox(const CheckboxConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto cb = std::make_shared<Checkbox>();
        cb->value = cfg.value;
        cb->active_color  = c.primary;
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

    // -----------------------------------------------------------------------
    // Radio
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildRadio(const RadioConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto rd = std::make_shared<Radio>(0);
        rd->active_color   = c.primary;
        rd->inactive_color = c.outline;

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = rd;
            return faded;
        }
        return rd;
    }

    // -----------------------------------------------------------------------
    // Slider — MD3 uses a notably thick (16dp) active track
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildSlider(const SliderConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sl = std::make_shared<Slider>();
        sl->value = cfg.value;
        sl->min   = cfg.min;
        sl->max   = cfg.max;
        sl->active_color   = c.primary;
        sl->inactive_color = c.surface_variant;
        sl->track_height   = 16.0f;
        sl->thumb_radius   = 10.0f;

        if (cfg.enabled && cfg.on_changed) {
            sl->on_changed = cfg.on_changed;
            return sl;
        }
        sl->on_changed = nullptr;
        auto faded = std::make_shared<Opacity>();
        faded->opacity = 0.4f;
        faded->child   = sl;
        return faded;
    }

    // -----------------------------------------------------------------------
    // TextField — MD3 Outlined Text Field: transparent fill, radius_xs border
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildTextField(const TextFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tf = std::make_shared<TextField>();
        tf->placeholder          = cfg.placeholder;
        tf->obscure_text         = cfg.obscure_text;
        tf->max_lines            = cfg.max_lines;
        tf->fill_color           = Color::transparent();
        tf->border_color         = c.outline;
        tf->focused_border_color = c.primary;
        tf->cursor_color         = c.primary;
        tf->selection_color      = withOpacity(c.primary, 0.3f);
        tf->placeholder_color    = c.on_surface_variant;
        tf->border_radius        = tokens_.shape.radius_xs;
        tf->border_width         = 1.0f;
        tf->min_height           = 56.0f; // MD3 outlined field default height

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
    // Card — MD3 shape token: Medium (12dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildCard(const CardConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        BoxDecoration deco;
        deco.border_radius = tokens_.shape.radius_md;

        switch (cfg.priority) {
            case CardPriority::filled:
                // Real M3 filled Card uses the surfaceContainerHighest
                // tonal role, not surface_variant — confirmed by sampling
                // a real capture (light: #E6E0E9, dark: #36343B). In light
                // theme surface_variant (#E7E0EC) happens to sit close
                // enough to pass within tolerance, which masked this in
                // that theme; dark's surface_variant (#49454F) diverges
                // far more, exposing the real mismatch.
                deco.color = (tokens_.brightness == Brightness::dark)
                    ? Color::fromARGB(0xFF36343B)
                    : Color::fromARGB(0xFFE6E0E9);
                break;
            case CardPriority::outlined:
                deco.color  = c.surface;
                deco.border = BoxBorder::all(c.outline, 1.0f);
                break;
            case CardPriority::elevated:
            default:
                // Real M3 ElevatedCard uses the surfaceContainerLow tonal
                // role, not plain surface — confirmed by sampling a real
                // capture (light: #F7F2FA, dark: #1D1B20), one tonal step
                // below buildPopupMenuButton()'s surfaceContainer. Same
                // reasoning as that fix: the shared ColorScheme has no
                // container-tier roles, so this is inlined here rather
                // than growing that struct for one role.
                deco.color = (tokens_.brightness == Brightness::dark)
                    ? Color::fromARGB(0xFF1D1B20)
                    : Color::fromARGB(0xFFF7F2FA);
                // MD3 elevated card resting elevation is level 1 (1dp).
                deco.box_shadow = {
                    BoxShadow{
                        Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.12f),
                        Offset{0.0f, tokens_.elevation.level1 * 0.5f},
                        tokens_.elevation.level1 * 2.0f
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

    WidgetRef MaterialDesignSystem::buildProgressIndicator(const ProgressConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        if (cfg.type == ProgressType::circular) {
            auto pi = std::make_shared<CircularProgressIndicator>();
            if (cfg.value.has_value()) pi->value = *cfg.value;
            pi->value_color      = c.primary;
            pi->background_color = c.surface_variant;
            pi->stroke_width     = 4.0f;
            return pi;
        }
        auto pi = std::make_shared<LinearProgressIndicator>();
        if (cfg.value.has_value()) pi->value = *cfg.value;
        pi->value_color      = c.primary;
        pi->background_color = c.surface_variant;
        pi->min_height       = 4.0f;
        return pi;
    }

    // -----------------------------------------------------------------------
    // Tooltip — MD3 shape token: Extra Small (4dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildTooltip(const TooltipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tt = std::make_shared<Tooltip>();
        tt->message          = cfg.message;
        tt->child             = cfg.child;
        tt->background_color = c.inverse_surface;
        tt->text_color       = c.inverse_on_surface;
        tt->border_radius    = tokens_.shape.radius_xs;
        tt->padding           = EdgeInsets::symmetric(8.0f, 4.0f);
        tt->font_size          = 12.0f;
        tt->display_duration_ms = 2500.0;
        return tt;
    }

    // -----------------------------------------------------------------------
    // ListTile
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildListTile(const ListTileConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        // Same fg-discard pattern found and fixed repeatedly this session:
        // cfg.leading arrives as plain caller content — real M3 ListItem
        // colors its leading icon onSurfaceVariant.
        WidgetRef leading = cfg.leading;
        if (auto asIcon = std::dynamic_pointer_cast<const Icon>(leading)) {
            leading = Icon::create(asIcon->texture, asIcon->size, c.on_surface_variant);
        }

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

        std::vector<WidgetRef> row_children;
        if (leading) {
            row_children.push_back(leading);
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

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
        padded->child   = row;

        const float min_h = cfg.subtitle ? 72.0f : 56.0f;
        const float inf   = std::numeric_limits<float>::infinity();
        auto constrained = std::make_shared<ConstrainedBox>();
        constrained->additional_constraints = BoxConstraints{0.0f, inf, min_h, inf};
        constrained->child = padded;

        // Real M3 ListItem paints an opaque containerColor by default
        // (ListItemDefaults.colors().containerColor == colorScheme.surface)
        // — unlike Flutter's ListTile convention of staying transparent and
        // relying on an ancestor Scaffold/Container for its background.
        // Confirmed by a real capture showing a solid surface-colored row
        // behind the text, previously invisible here since this widget
        // painted nothing at all.
        auto container = std::make_shared<Container>();
        container->color = c.surface;
        container->child = constrained;

        WidgetRef result = container;
        if ((cfg.on_tap || cfg.on_long_press) && cfg.enabled) {
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap        = cfg.on_tap;
            gesture->on_long_press = cfg.on_long_press;
            gesture->child         = result;
            result                 = gesture;
        }

        if (!cfg.enabled) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = result;
            return faded;
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // Divider — MD3 uses outline-variant, not outline, for dividers
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildDivider(const DividerConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto line = std::make_shared<Container>();
        line->height = 1.0f;
        line->color  = c.outline_variant;

        auto indented = std::make_shared<Padding>();
        indented->padding = EdgeInsets::only(cfg.indent, 0.0f, cfg.end_indent, 0.0f);
        indented->child   = line;

        auto box = std::make_shared<SizedBox>();
        box->height = 16.0f;
        box->child  = std::make_shared<Align>(Alignment::center(), indented);
        return box;
    }

    // -----------------------------------------------------------------------
    // AppBar — MD3 top app bar: flat surface container
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildAppBar(const AppBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        // Same fg-discard pattern found and fixed repeatedly this session:
        // cfg.leading/actions arrive as plain caller content with no
        // expectation of being pre-tinted — real M3 TopAppBar colors its
        // navigation icon onSurface and its action icons
        // onSurfaceVariant.
        auto tint_icon = [](WidgetRef w, Color tint) -> WidgetRef {
            if (auto asIcon = std::dynamic_pointer_cast<const Icon>(w)) {
                return Icon::create(asIcon->texture, asIcon->size, tint);
            }
            return w;
        };

        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(tint_icon(cfg.leading, c.on_surface));
            row_children.push_back(SizedBox::from_width(12.0f));
        }
        if (cfg.title) {
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(cfg.title)));
        } else {
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(SizedBox::shrink())));
        }
        for (const auto& action : cfg.actions) {
            row_children.push_back(SizedBox::from_width(8.0f));
            row_children.push_back(tint_icon(action, c.on_surface_variant));
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
    // NavigationBar — MD3 selected item gets a pill-shaped tonal indicator
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildNavigationBar(const NavigationBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;

            // Same fg-discard pattern found and fixed repeatedly this
            // session: item.icon arrives as plain caller content with no
            // expectation of being pre-tinted for the selected state —
            // real M3 NavigationBar tints on_secondary_container when
            // selected (against the pill below) and on_surface_variant
            // otherwise. Not yet validated against a real capture (this
            // builder isn't in androidBuilders() yet), but the same
            // correctness fix as the (validated) navigationRail case
            // right below.
            WidgetRef icon_widget = item.icon;
            const Color navbar_icon_tint = selected ? c.on_secondary_container : c.on_surface_variant;
            if (auto asIcon = std::dynamic_pointer_cast<const Icon>(icon_widget)) {
                icon_widget = Icon::create(asIcon->texture, asIcon->size, navbar_icon_tint);
            }
            if (icon_widget && selected) {
                // MD3 active indicator: a pill-shaped secondaryContainer
                // behind the icon.
                BoxDecoration pill;
                pill.color         = c.secondary_container;
                pill.border_radius = tokens_.shape.radius_full;
                auto indicator = std::make_shared<DecoratedBox>();
                indicator->decoration = pill;
                auto padded_icon = std::make_shared<Padding>();
                padded_icon->padding = EdgeInsets::symmetric(16.0f, 4.0f);
                padded_icon->child   = icon_widget;
                indicator->child = padded_icon;
                icon_widget = indicator;
            }

            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::center;
            if (icon_widget) col->children.push_back(icon_widget);
            if (!item.label.empty()) {
                TextStyle ts;
                ts.font_size = 12.0f;
                ts.color = c.on_surface;
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

        // Real M3 NavigationBar uses the surfaceContainer tonal role, not
        // plain surface — confirmed by sampling a real Compose capture
        // (light: #F3EDF7, dark: #211F26), same values already used by
        // buildPopupMenuButton()'s DropdownMenu panel above. Without this,
        // the bar rendered with no visible background at all since
        // c.surface is indistinguishable from the page background.
        const Color surface_container = (tokens_.brightness == Brightness::dark)
            ? Color::fromARGB(0xFF211F26)
            : Color::fromARGB(0xFFF3EDF7);

        auto container = std::make_shared<Container>();
        container->color = surface_container;
        container->child = row;
        return container;
    }

    // -----------------------------------------------------------------------
    // Dialog — MD3 shape token: Extra Large (28dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildDialog(const DialogConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> children;

        // cfg.title/cfg.content arrive as plain caller Text (see
        // themed_component_harness.cpp's shared "dialog" case, default
        // TextStyle — no expectation of being pre-sized for this design
        // system), but never had M3's real dialog typography applied —
        // confirmed against a real capture showing the title rendering at
        // roughly body size instead of the real headlineSmall. Real M3:
        // title -> headlineSmall/onSurface, content -> bodyMedium/
        // onSurfaceVariant.
        auto restyle = [](WidgetRef widget, TextStyle style) -> WidgetRef {
            if (auto text = std::dynamic_pointer_cast<const Text>(widget)) {
                return std::make_shared<Text>(text->span.text, style);
            }
            return widget;
        };

        if (cfg.title) {
            TextStyle title_style = tokens_.typography.headline_small;
            title_style.withColor(c.on_surface);
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(24.0f, 24.0f, 24.0f, 0.0f);
            padded->child   = restyle(cfg.title, title_style);
            children.push_back(padded);
        }
        if (cfg.content) {
            TextStyle content_style = tokens_.typography.body_medium;
            content_style.withColor(c.on_surface_variant);
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(24.0f, 16.0f, 24.0f, 0.0f);
            padded->child   = restyle(cfg.content, content_style);
            children.push_back(padded);
        }
        if (!cfg.actions.empty()) {
            children.push_back(SizedBox::from_height(24.0f));
            // Same fg-discard pattern already found in buildButton()/
            // buildChip()/buildSegmentedButton(): a real M3 dialog's
            // TextButton actions default to colorScheme.primary, but
            // cfg.actions arrives as plain caller widgets (see
            // themed_component_harness.cpp's shared "dialog" case, whose
            // black-default styling was tuned for iOS, not Material) with
            // no expectation of being pre-colored for this design system.
            // Real M3 TextButtons reserve a 40dp minimum height (plus 24dp
            // bottom margin below the whole row) even though the label text
            // itself is much shorter — confirmed against a real capture
            // whose dialog card ran ~37dp taller than ours despite matching
            // title/content layout, entirely accounted for by this row's
            // height once fixed. Plain caller Text has no such reservation,
            // so each action is centered inside a fixed-height box here.
            auto tint_action = [&](WidgetRef action) -> WidgetRef {
                WidgetRef styled = action;
                if (auto text = std::dynamic_pointer_cast<const Text>(action)) {
                    if (text->span.style.color == Color::black()) {
                        TextStyle style = text->span.style;
                        style.withColor(c.primary);
                        styled = std::make_shared<Text>(text->span.text, style);
                    }
                }
                // SizedBox::from_height() alone leaves width unset, which
                // means "fill available width" — inside the Row that made
                // every action balloon out to the Row's full remaining
                // width, so Center ended up centering the label mid-row
                // instead of hugging the trailing edge. IntrinsicWidth
                // doesn't help either: it measures by laying the child out
                // unconstrained, and an unconstrained SizedBox(width=
                // nullopt) reports infinity, not its child's natural width.
                // Align's width_factor is the actual "shrink-wrap to child"
                // primitive; pairing it with a tight-height ConstrainedBox
                // shrink-wraps width while still forcing the 40dp height.
                auto align = std::make_shared<Align>();
                align->alignment    = Alignment::center();
                align->width_factor = 1.0f;
                align->child        = styled;

                auto constrained = std::make_shared<ConstrainedBox>();
                constrained->additional_constraints =
                    BoxConstraints{0.0f, std::numeric_limits<float>::infinity(), 40.0f, 40.0f};
                constrained->child = align;
                return constrained;
            };
            std::vector<WidgetRef> tinted_actions;
            tinted_actions.reserve(cfg.actions.size());
            for (const auto& action : cfg.actions) tinted_actions.push_back(tint_action(action));

            auto row = std::make_shared<Row>();
            row->main_axis_alignment = MainAxisAlignment::end;
            row->children = std::move(tinted_actions);
            auto action_pad = std::make_shared<Padding>();
            action_pad->padding = EdgeInsets::only(16.0f, 0.0f, 24.0f, 24.0f);
            action_pad->child   = row;
            children.push_back(action_pad);
        } else {
            children.push_back(SizedBox::from_height(24.0f));
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        // Real M3 AlertDialog containers use the surfaceContainerHigh tonal
        // role, not plain surface — confirmed by sampling a real Compose
        // AlertDialog capture (light: #ECE6F0, dark: #2B2930, both exact
        // matches for the M3 baseline palette's surfaceContainerHigh). The
        // shared ColorScheme has no container-tier roles, so these are
        // inlined here rather than growing that cross-design-system struct
        // for a single role only Material dialogs need.
        const Color surface_container_high = (tokens_.brightness == Brightness::dark)
            ? Color::fromARGB(0xFF2B2930)
            : Color::fromARGB(0xFFECE6F0);

        auto dialog = std::make_shared<Dialog>();
        dialog->child             = col;
        dialog->background_color  = surface_container_high;
        dialog->border_radius     = tokens_.shape.radius_xl; // MD3: Extra Large
        dialog->elevation         = tokens_.elevation.level3;
        dialog->max_width         = 560.0f; // MD3 default basic dialog max width
        return dialog;
    }

    // -----------------------------------------------------------------------
    // SnackBar — MD3 shape token: Extra Small (4dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildSnackBar(const SnackBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        TextStyle msg_style;
        msg_style.font_size = 14.0f;
        msg_style.color = c.inverse_on_surface;
        auto msg_text = std::make_shared<Text>(cfg.message, msg_style);

        std::vector<WidgetRef> row_children;
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(msg_text)));

        if (cfg.action_label.has_value() && cfg.on_action) {
            TextStyle action_style;
            action_style.font_size = 14.0f;
            action_style.color = c.inverse_primary;
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
        sb->content          = row;
        sb->background_color = c.inverse_surface;
        sb->border_radius    = tokens_.shape.radius_xs;
        sb->padding          = EdgeInsets::symmetric(16.0f, 14.0f);
        sb->duration_ms      = cfg.duration_ms;
        return sb;
    }

    // -----------------------------------------------------------------------
    // PopupMenuButton — MD3 shape token: Extra Small (4dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildPopupMenuButton(const PopupMenuConfig& cfg) const
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

        // Real M3 DropdownMenu uses the surfaceContainer tonal role, not
        // plain surface — confirmed by sampling a real Compose capture
        // (light: #F3EDF7, dark: #211F26, both exact matches for the M3
        // baseline palette's surfaceContainer — one tonal step below
        // buildDialog()'s surfaceContainerHigh). Same reasoning as that
        // fix: the shared ColorScheme has no container-tier roles, so this
        // is inlined here rather than growing that struct for one role.
        const Color surface_container = (tokens_.brightness == Brightness::dark)
            ? Color::fromARGB(0xFF211F26)
            : Color::fromARGB(0xFFF3EDF7);

        auto pmb = std::make_shared<PopupMenuButton>();
        pmb->items         = std::move(items);
        pmb->on_selected   = cfg.on_selected;
        pmb->child         = cfg.child;
        pmb->popup_color     = surface_container;
        pmb->border_radius   = tokens_.shape.radius_xs;
        pmb->elevation       = tokens_.elevation.level2;
        // Real M3 DropdownMenu enforces a 112dp minimum width — confirmed
        // against a real capture (two one-word items rendered at ~318px
        // physical / 2.625 DPR ≈ 118dp, matching spec within antialiasing
        // noise) rather than shrinking to the "One"/"Two" labels' own
        // much narrower natural width.
        pmb->menu_min_width  = 112.0f;
        return pmb;
    }

    // -----------------------------------------------------------------------
    // DropdownButton — MD3 shape token: Extra Small (4dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildDropdownButton(const DropdownConfig& cfg) const
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
        dd->border_radius  = tokens_.shape.radius_xs;
        dd->elevation      = tokens_.elevation.level2;
        return dd;
    }

    // -----------------------------------------------------------------------
    // PrimaryActionButton (FAB) — MD3 default FAB is a 16dp-radius rounded
    // square, NOT circular (a deliberate MD3 departure from Material 2).
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildPrimaryActionButton(const PrimaryActionConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        const float diameter = 56.0f;

        BoxDecoration deco;
        // MD3's actual default FAB color is primaryContainer, not primary —
        // a real spec detail easy to miss without the container role.
        deco.color         = c.primary_container;
        deco.border_radius = tokens_.shape.radius_lg; // MD3 FAB shape: Large (16dp)
        const float el = tokens_.elevation.level3;
        deco.box_shadow = {
            BoxShadow{
                Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                Offset{0.0f, el * 0.5f},
                el * 2.0f
            }
        };

        WidgetRef content;
        if (cfg.icon) {
            // Same fg-discard pattern found and fixed repeatedly this
            // session: cfg.icon arrives as plain caller content — real
            // M3 FAB icon color is onPrimaryContainer, matching the
            // fallback "+" glyph's own color just below.
            if (auto asIcon = std::dynamic_pointer_cast<const Icon>(cfg.icon)) {
                content = Icon::create(asIcon->texture, asIcon->size, c.on_primary_container);
            } else {
                content = cfg.icon;
            }
        } else if (cfg.label) {
            content = cfg.label;
        } else {
            content = std::make_shared<Text>("+",
                TextStyle{}.withFontSize(24.0f).withColor(c.on_primary_container));
        }

        // Align must be the *inner* widget, SizedBox outermost — see
        // campello_ui's buildPrimaryActionButton() for the full
        // explanation of why the reverse nesting silently expands to fill
        // available space instead of staying pinned to 56x56.
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
    // TabBar — MD3 active indicator thickness is 3dp
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildTabBar(const TabBarConfig& cfg) const
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
        tb->indicator_color        = c.primary;
        tb->label_color            = c.primary;
        tb->unselected_label_color = c.on_surface_variant;
        tb->indicator_weight       = 3.0f;
        tb->background_color       = c.surface;
        return tb;
    }

    // -----------------------------------------------------------------------
    // Chip — MD3 shape token: Small (8dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildChip(const ChipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        // MD3's real selected-Filter-Chip treatment is secondaryContainer,
        // not a primary tint.
        Color bg     = cfg.selected ? c.secondary_container : c.surface;
        Color fg     = cfg.selected ? c.on_secondary_container : c.on_surface_variant;
        Color border = cfg.selected ? Color::transparent() : c.outline;

        // Same real bug as buildButton() had: `fg` was computed (selected ->
        // on_secondary_container, unselected -> on_surface_variant) but
        // never applied to cfg.label, which arrives as plain caller text
        // with no color set (see themed_component_harness.cpp's
        // `cfg.label = text("Chip")`) — so the label always rendered in
        // TextStyle's untouched default (black) regardless of chip state.
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
            row_children.push_back(SizedBox::from_width(8.0f));
        }
        row_children.push_back(tint_label(cfg.label));
        if (cfg.on_deleted) {
            row_children.push_back(SizedBox::from_width(8.0f));
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
        deco.border_radius = tokens_.shape.radius_sm;
        if (!cfg.selected) deco.border = BoxBorder::all(border, 1.0f);

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
    // SegmentedButton — MD3's real look: a connected button group with a
    // stadium outer border and per-segment dividers, not a sliding pill
    // (that's Cupertino's UISegmentedControl instead).
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildSegmentedButton(const SegmentedConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        // Same fg-discard pattern already found and fixed in buildButton()/
        // buildChip(): seg.label arrives as plain caller text (see
        // themed_component_harness.cpp's `text("Day")`, default black) with
        // no expectation that it's pre-colored for this segment's state.
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
            // Real MD3 SegmentedButton: selected label ->
            // onSecondaryContainer, unselected -> onSurface.
            const Color fg = selected ? c.on_secondary_container : c.on_surface;

            std::vector<WidgetRef> content_children;
            // Real M3 SegmentedButton shows a checkmark in place of a
            // custom icon when checked and none was supplied — confirmed
            // against a real capture (segmentedButton_three_segments's
            // "Day" segment renders a checkmark, not just a tinted
            // background).
            if (selected && !seg.icon) {
                content_children.push_back(std::make_shared<Text>(
                    "✓", TextStyle{}.withFontSize(16.0f).withColor(fg)));
                content_children.push_back(SizedBox::from_width(8.0f));
            } else if (seg.icon) {
                content_children.push_back(seg.icon);
            }
            if (seg.label) content_children.push_back(tint_label(seg.label, fg));
            auto content_row = std::make_shared<Row>();
            content_row->main_axis_alignment = MainAxisAlignment::center;
            content_row->main_axis_size = MainAxisSize::min;
            content_row->children = std::move(content_children);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(12.0f, 8.0f);
            padded->child   = content_row;

            BoxDecoration deco;
            // MD3's real selected-segment treatment is secondaryContainer.
            deco.color = selected ? c.secondary_container : Color::transparent();
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
        outer.border_radius = tokens_.shape.radius_full; // stadium outer shape
        outer.border        = BoxBorder::all(c.outline, 1.0f);
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        // DecoratedBox's own rounded fill/border don't clip child painting
        // (see render_decorated_box.cpp) — without this, the first/last
        // segment's square-cornered secondaryContainer fill pokes past the
        // stadium outline's rounded corners. Confirmed against a real
        // capture: the real SegmentedButton's own first/last items get an
        // itemShape with a rounded outer corner; ClipRRect reproduces that
        // here without needing per-corner radius support in BoxDecoration.
        decorated->child = std::make_shared<ClipRRect>(tokens_.shape.radius_full, container_row);

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
    // BottomSheet — MD3 shape token: Extra Large top (28dp)
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildBottomSheet(const BottomSheetConfig& cfg) const
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
        deco.border_radius = tokens_.shape.radius_xl;
        const float el = tokens_.elevation.level1;
        deco.box_shadow = {
            BoxShadow{
                Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.12f),
                Offset{0.0f, -el},
                el * 3.0f
            }
        };
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // Badge — MD3's small dot is 6dp; the "large" numbered badge is a pill.
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildBadge(const BadgeConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        WidgetRef badge_content;
        float diameter = 6.0f;
        if (cfg.label.has_value()) {
            diameter = 16.0f;
            TextStyle ts;
            ts.font_size = 11.0f;
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
        positioned->right = -2.0f;
        positioned->top   = -2.0f;
        positioned->child = sized_badge;

        auto stack = std::make_shared<Stack>();
        stack->children = {cfg.child, positioned};
        return stack;
    }

    // -----------------------------------------------------------------------
    // IconButton — MD3 toggle icon button: solid primary fill when selected.
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildIconButton(const IconButtonConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        // Same fg-discard pattern found and fixed repeatedly this session:
        // cfg.icon arrives as plain caller content (a template Icon with
        // whatever default color it was constructed with) with no
        // expectation of being pre-tinted for the selected state — real
        // M3 IconButton tints on_primary when selected (against the solid
        // primary fill below) and on_surface_variant otherwise.
        WidgetRef tinted_icon = cfg.icon;
        const Color icon_tint = cfg.selected ? c.on_primary : c.on_surface_variant;
        if (auto asIcon = std::dynamic_pointer_cast<const Icon>(cfg.icon)) {
            tinted_icon = Icon::create(asIcon->texture, asIcon->size, icon_tint);
        }

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::all(8.0f);
        padded->child   = tinted_icon;

        BoxDecoration deco;
        deco.color         = cfg.selected ? c.primary : Color::transparent();
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
    // Stepper — no first-class MD3 component (that's UIStepper's real
    // home); styled as small tonal Small-shape (8dp) buttons.
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildStepper(const StepperConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto makeButton = [&](const std::string& glyph, std::function<void()> on_tap) {
            BoxDecoration deco;
            deco.color         = withOpacity(c.primary, 0.12f);
            deco.border_radius = tokens_.shape.radius_sm;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(12.0f, 8.0f);
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

    WidgetRef MaterialDesignSystem::buildRatingIndicator(const RatingConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> stars;
        for (int i = 0; i < cfg.max; ++i) {
            bool filled = i < cfg.value;
            auto glyph = std::make_shared<Text>(filled ? "*" : "-",
                TextStyle{}.withFontSize(18.0f).withColor(filled ? c.primary : c.outline));
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
    // ActionSheet — MD3's real equivalent is a modal bottom sheet with a
    // plain list of items. Unlike iOS, MD3 has no "detached Cancel button"
    // convention — dismissal is by tapping outside or the back gesture, so
    // on_cancel (if set) renders as an ordinary list row, not a separated
    // button.
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildActionSheet(const ActionSheetConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> children;

        if (cfg.title) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(16.0f, 16.0f, 16.0f, 8.0f);
            padded->child   = cfg.title;
            children.push_back(padded);
        }

        auto addRow = [&](const std::string& label, std::function<void()> on_tap, Color color) {
            TextStyle ts;
            ts.font_size = 14.0f;
            ts.color = color;
            auto text = std::make_shared<Text>(label, ts);
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
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

        BoxDecoration deco;
        deco.color         = c.surface;
        deco.border_radius = tokens_.shape.radius_xl; // matches BottomSheet
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // SearchField — MD3 Search shape token: full corner (pill).
    // -----------------------------------------------------------------------

    WidgetRef MaterialDesignSystem::buildSearchField(const SearchFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto icon = std::make_shared<Text>("search",
            TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));

        auto controller = std::make_shared<TextEditingController>(cfg.value);
        auto tf = std::make_shared<TextField>(controller, cfg.placeholder);
        tf->fill_color        = Color::transparent();
        tf->border_color      = Color::transparent();
        tf->placeholder_color = c.on_surface_variant;
        tf->min_height        = 44.0f;
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
        padded->padding = EdgeInsets::symmetric(16.0f, 4.0f);
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = c.surface_variant;
        deco.border_radius = tokens_.shape.radius_full;
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
    // DatePicker / TimePicker — styled like MD3's outlined text field, with
    // radius_xs (matching buildTextField); chrome only, see doc comment on
    // DatePickerConfig.
    // -----------------------------------------------------------------------

    namespace
    {
        WidgetRef buildTriggerField(const DesignTokens& tokens, const std::string& glyph,
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
                SizedBox::from_width(8.0f),
                icon_text,
            };

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
            padded->child   = row;

            BoxDecoration deco;
            deco.border        = BoxBorder::all(c.outline, 1.0f);
            deco.border_radius = tokens.shape.radius_xs;
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

    WidgetRef MaterialDesignSystem::buildDatePicker(const DatePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "date", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef MaterialDesignSystem::buildTimePicker(const TimePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "time", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef MaterialDesignSystem::buildExpansionTile(const ExpansionTileConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        std::vector<WidgetRef> header_children;
        if (cfg.leading) {
            header_children.push_back(cfg.leading);
            header_children.push_back(SizedBox::from_width(16.0f));
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
        // MD3 uses a rotating chevron; we approximate with glyph swap since
        // there's no rotation transform primitive wired through this config.
        header_children.push_back(std::make_shared<Text>(cfg.expanded ? "^" : "v",
            TextStyle{}.withFontSize(13.0f).withColor(c.on_surface_variant)));

        auto header_row = std::make_shared<Row>();
        header_row->cross_axis_alignment = CrossAxisAlignment::center;
        header_row->children = std::move(header_children);

        auto header_padded = std::make_shared<Padding>();
        header_padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
        header_padded->child   = header_row;

        auto header_gesture = std::make_shared<GestureDetector>();
        if (cfg.enabled && cfg.on_expansion_changed) {
            header_gesture->on_tap = [cb = cfg.on_expansion_changed, expanded = cfg.expanded] { cb(!expanded); };
        }
        header_gesture->child = header_padded;

        std::vector<WidgetRef> children = {header_gesture};
        if (cfg.expanded && cfg.children_content) {
            children.push_back(cfg.children_content);
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

    WidgetRef MaterialDesignSystem::buildToggleButtons(const ToggleButtonsConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        // Real M3 ToggleButtons is functionally a multi-select
        // SegmentedButtonRow (MultiChoiceSegmentedButtonRow +
        // SegmentedButton in Compose) — mirrors buildSegmentedButton()'s
        // shared stadium outline (one outer border, dividers only between
        // items, not each item individually boxed) and checkmark-when-
        // checked treatment, confirmed missing against a real capture.
        auto tint_label = [&](WidgetRef label, Color fg) -> WidgetRef {
            if (auto text = std::dynamic_pointer_cast<const Text>(label)) {
                TextStyle style = text->span.style;
                style.withColor(fg);
                return std::make_shared<Text>(text->span.text, style);
            }
            return label;
        };

        std::vector<WidgetRef> items;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool is_last = i + 1 == cfg.items.size();
            const Color fg = item.selected ? c.on_secondary_container : c.on_surface;

            std::vector<WidgetRef> content_children;
            if (item.selected && !item.icon) {
                content_children.push_back(std::make_shared<Text>(
                    "✓", TextStyle{}.withFontSize(16.0f).withColor(fg)));
                content_children.push_back(SizedBox::from_width(8.0f));
            } else if (item.icon) {
                content_children.push_back(item.icon);
            }
            if (item.label) content_children.push_back(tint_label(item.label, fg));
            auto content_row = std::make_shared<Row>();
            content_row->main_axis_alignment = MainAxisAlignment::center;
            content_row->main_axis_size = MainAxisSize::min;
            content_row->children = std::move(content_children);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 10.0f);
            padded->child   = content_row;

            // MD3 ToggleButtons: selected segment gets a filled
            // secondaryContainer background, matching the SegmentedButton
            // convention used elsewhere in this file.
            BoxDecoration deco;
            deco.color = item.selected ? c.secondary_container : Color::transparent();
            if (!is_last) deco.border = BoxBorder::all(c.outline, 1.0f);
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
        row->cross_axis_alignment = CrossAxisAlignment::stretch;
        row->children = std::move(items);

        BoxDecoration outer;
        outer.border_radius = tokens_.shape.radius_full; // stadium outer shape
        outer.border        = BoxBorder::all(c.outline, 1.0f);
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        // Same DecoratedBox-doesn't-clip-its-child issue fixed in
        // buildSegmentedButton() above — without it the first/last item's
        // square-cornered fill pokes past the stadium outline's corners.
        decorated->child = std::make_shared<ClipRRect>(tokens_.shape.radius_full, row);

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

    WidgetRef MaterialDesignSystem::buildBanner(const BannerConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(16.0f));
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

        std::vector<WidgetRef> col_children = {padded};
        auto bottom_divider = std::make_shared<Container>();
        bottom_divider->height = 1.0f;
        bottom_divider->color  = c.outline_variant;
        col_children.push_back(bottom_divider);

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(col_children);

        // MD3 banners sit on plain surface, not surfaceVariant — they're
        // distinguished by the divider + leading icon, not a tinted fill.
        BoxDecoration deco;
        deco.color = c.surface;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    WidgetRef MaterialDesignSystem::buildNavigationRail(const NavigationRailConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;

            // Same fg-discard pattern as buildIconButton()/buildNavigationBar()
            // above: real M3 NavigationRail tints the icon
            // on_secondary_container when selected (against the pill
            // below) and on_surface_variant otherwise.
            WidgetRef icon_widget = item.icon;
            const Color rail_icon_tint = selected ? c.on_secondary_container : c.on_surface_variant;
            if (auto asIcon = std::dynamic_pointer_cast<const Icon>(icon_widget)) {
                icon_widget = Icon::create(asIcon->texture, asIcon->size, rail_icon_tint);
            }
            if (icon_widget && selected) {
                // Same pill-indicator convention as buildNavigationBar.
                BoxDecoration pill;
                pill.color         = c.secondary_container;
                pill.border_radius = tokens_.shape.radius_full;
                auto indicator = std::make_shared<DecoratedBox>();
                indicator->decoration = pill;
                auto padded_icon = std::make_shared<Padding>();
                padded_icon->padding = EdgeInsets::symmetric(16.0f, 8.0f);
                padded_icon->child   = icon_widget;
                indicator->child = padded_icon;
                icon_widget = indicator;
            }

            std::vector<WidgetRef> content;
            if (icon_widget) content.push_back(icon_widget);
            // Real M3 NavigationRailItem defaults alwaysShowLabel=true —
            // even the compact (non-extended) rail shows the label below
            // the icon; cfg.extended only changes the layout (label beside
            // the icon in a Row vs. stacked below it in a Column), not
            // whether the label exists at all. Confirmed against a real
            // capture showing "Home"/"Search"/"Profile" labels in compact
            // mode, previously dropped entirely by this condition.
            if (!item.label.empty()) {
                content.push_back(cfg.extended ? SizedBox::from_width(12.0f)
                                                : SizedBox::from_height(4.0f));
                TextStyle ts;
                ts.font_size = 13.0f;
                ts.color = c.on_surface;
                if (selected) ts.font_weight = FontWeight::bold;
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
            padded->padding = EdgeInsets::symmetric(cfg.extended ? 16.0f : 8.0f, 8.0f);
            padded->child   = entry_content;

            WidgetRef entry = padded;
            if (cfg.on_tap) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_tap, idx = static_cast<int>(i)] { cb(idx); };
                gesture->child  = padded;
                entry = gesture;
            }
            item_widgets.push_back(entry);
            if (i + 1 < cfg.items.size()) item_widgets.push_back(SizedBox::from_height(12.0f));
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = cfg.extended ? CrossAxisAlignment::stretch : CrossAxisAlignment::center;
        col->children = std::move(item_widgets);

        auto padded_col = std::make_shared<Padding>();
        padded_col->padding = EdgeInsets::symmetric(0.0f, 16.0f);
        padded_col->child   = col;

        BoxDecoration outer;
        outer.color = c.surface;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = padded_col;

        // Real M3 NavigationRail's compact width is a fixed 80dp per spec,
        // not content-driven — confirmed against a real capture (exactly
        // 80.0dp), vs. this widget's previous organic shrink-to-content
        // width (~90dp, coincidentally close but not the actual spec
        // value). Extended rail width stays content-driven, matching its
        // own spec (label-width-dependent, no single fixed value).
        if (!cfg.extended) {
            return SizedBox::from_width(80.0f, decorated);
        }
        return decorated;
    }

    WidgetRef MaterialDesignSystem::buildDataTable(const DataTableConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto makeCell = [&](WidgetRef content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 12.0f);
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
        outer.color = c.surface;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = col;
        return decorated;
    }

} // namespace systems::leal::campello_widgets
