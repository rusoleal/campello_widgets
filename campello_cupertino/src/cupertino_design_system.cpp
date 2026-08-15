#include <campello_cupertino/cupertino_design_system.hpp>

// Widgets
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/backdrop_filter.hpp>
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

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    Color CupertinoDesignSystem::withOpacity(Color c, float opacity)
    {
        return Color::fromRGBA(c.r, c.g, c.b, c.a * opacity);
    }

    namespace
    {
        // iOS defines 11 standard Dynamic Type styles; our TypographyScale
        // has 15 slots (5 tiers x 3 sizes, shaped after MD3). Mapped in
        // descending size order, reusing the nearest iOS style per slot —
        // some HIG sizes necessarily repeat across adjacent slots.
        TypographyScale makeTypography(Color label)
        {
            TypographyScale t;
            t.display_large   = TextStyle{}.withFontSize(34.0f).withColor(label); // Large Title
            t.display_medium  = TextStyle{}.withFontSize(28.0f).withColor(label); // Title 1
            t.display_small   = TextStyle{}.withFontSize(22.0f).withColor(label); // Title 2
            t.headline_large  = TextStyle{}.withFontSize(20.0f).withColor(label); // Title 3
            t.headline_medium = TextStyle{}.withFontSize(17.0f).bold().withColor(label); // Headline
            t.headline_small  = TextStyle{}.withFontSize(16.0f).bold().withColor(label); // Callout, emphasized
            t.title_large     = TextStyle{}.withFontSize(17.0f).withColor(label); // Body
            t.title_medium    = TextStyle{}.withFontSize(15.0f).withColor(label); // Subheadline
            t.title_small     = TextStyle{}.withFontSize(13.0f).withColor(label); // Footnote
            t.body_large      = TextStyle{}.withFontSize(17.0f).withColor(label); // Body
            t.body_medium     = TextStyle{}.withFontSize(15.0f).withColor(label); // Subheadline
            t.body_small      = TextStyle{}.withFontSize(13.0f).withColor(label); // Footnote
            t.label_large     = TextStyle{}.withFontSize(13.0f).bold().withColor(label); // Footnote, emphasized
            t.label_medium    = TextStyle{}.withFontSize(12.0f).bold().withColor(label); // Caption 1, emphasized
            t.label_small     = TextStyle{}.withFontSize(11.0f).bold().withColor(label); // Caption 2, emphasized
            return t;
        }

        DesignTokens makeLightTokens()
        {
            DesignTokens t;
            t.brightness = Brightness::light;

            t.colors.primary            = Color::fromARGB(0xFF007AFF); // systemBlue
            t.colors.on_primary         = Color::fromARGB(0xFFFFFFFF);
            t.colors.primary_container    = Color::fromARGB(0xFFD6E9FF); // light tint, matching the "tinted" button style above
            t.colors.on_primary_container = Color::fromARGB(0xFF00366B);
            t.colors.secondary          = Color::fromARGB(0xFF5856D6); // systemIndigo
            t.colors.on_secondary       = Color::fromARGB(0xFFFFFFFF);
            t.colors.secondary_container    = Color::fromARGB(0xFFE3E2FA);
            t.colors.on_secondary_container = Color::fromARGB(0xFF211F5B);
            // Tertiary — systemPink, HIG's usual "third" accent alongside blue/indigo.
            t.colors.tertiary           = Color::fromARGB(0xFFFF2D55); // systemPink
            t.colors.on_tertiary        = Color::fromARGB(0xFFFFFFFF);
            t.colors.tertiary_container    = Color::fromARGB(0xFFFFDDE4);
            t.colors.on_tertiary_container = Color::fromARGB(0xFF660019);
            t.colors.surface            = Color::fromARGB(0xFFFFFFFF); // systemBackground
            t.colors.on_surface         = Color::fromARGB(0xFF000000); // label
            t.colors.surface_variant    = Color::fromARGB(0xFFF2F2F7); // secondarySystemBackground
            t.colors.on_surface_variant = Color::fromARGB(0xFF6C6C70); // secondaryLabel (approx)
            t.colors.background         = Color::fromARGB(0xFFFFFFFF);
            t.colors.on_background      = Color::fromARGB(0xFF000000);
            t.colors.error              = Color::fromARGB(0xFFFF3B30); // systemRed
            t.colors.on_error           = Color::fromARGB(0xFFFFFFFF);
            t.colors.error_container    = Color::fromARGB(0xFFFFDAD6);
            t.colors.on_error_container = Color::fromARGB(0xFF410002);
            t.colors.success            = Color::fromARGB(0xFF34C759); // systemGreen
            t.colors.on_success         = Color::fromARGB(0xFFFFFFFF);
            t.colors.warning            = Color::fromARGB(0xFFFF9500); // systemOrange
            t.colors.on_warning         = Color::fromARGB(0xFFFFFFFF);
            t.colors.outline            = Color::fromARGB(0xFFC6C6C8); // opaqueSeparator
            t.colors.outline_variant    = Color::fromARGB(0xFFE5E5EA); // systemGray5
            t.colors.shadow             = Color::fromARGB(0xFF000000);
            t.colors.scrim              = Color::fromARGB(0x80000000);
            t.colors.inverse_surface    = Color::fromARGB(0xFF1C1C1E);
            t.colors.inverse_on_surface = Color::fromARGB(0xFFFFFFFF);
            t.colors.inverse_primary    = Color::fromARGB(0xFF0A84FF);

            // iOS shape scale — deliberately not MD3's stadium-heavy scale.
            t.shape.radius_xs = 6.0f;  // popovers/tooltips
            t.shape.radius_sm = 8.0f;  // small controls (checkbox glyph)
            t.shape.radius_md = 10.0f; // text fields, list rows, menus
            t.shape.radius_lg = 14.0f; // buttons, cards, alerts
            t.shape.radius_xl = 20.0f; // large sheets

            // iOS avoids Material-style elevation shadows almost entirely —
            // a much flatter, softer scale than MD3's 0/1/3/6/8/12dp.
            t.elevation = ElevationTokens{0.0f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f};

            t.typography = makeTypography(t.colors.on_surface);
            return t;
        }

        DesignTokens makeDarkTokens()
        {
            DesignTokens t;
            t.brightness = Brightness::dark;

            t.colors.primary            = Color::fromARGB(0xFF0A84FF);
            t.colors.on_primary         = Color::fromARGB(0xFFFFFFFF);
            t.colors.primary_container    = Color::fromARGB(0xFF00325C);
            t.colors.on_primary_container = Color::fromARGB(0xFFD6E9FF);
            t.colors.secondary          = Color::fromARGB(0xFF5E5CE6);
            t.colors.on_secondary       = Color::fromARGB(0xFFFFFFFF);
            t.colors.secondary_container    = Color::fromARGB(0xFF2E2C6B);
            t.colors.on_secondary_container = Color::fromARGB(0xFFE3E2FA);
            t.colors.tertiary           = Color::fromARGB(0xFFFF375F); // systemPink (dark)
            t.colors.on_tertiary        = Color::fromARGB(0xFFFFFFFF);
            t.colors.tertiary_container    = Color::fromARGB(0xFF66001F);
            t.colors.on_tertiary_container = Color::fromARGB(0xFFFFDDE4);
            t.colors.surface            = Color::fromARGB(0xFF000000);
            t.colors.on_surface         = Color::fromARGB(0xFFFFFFFF);
            t.colors.surface_variant    = Color::fromARGB(0xFF1C1C1E);
            t.colors.on_surface_variant = Color::fromARGB(0xFFAEAEB2);
            t.colors.background         = Color::fromARGB(0xFF000000);
            t.colors.on_background      = Color::fromARGB(0xFFFFFFFF);
            t.colors.error              = Color::fromARGB(0xFFFF453A);
            t.colors.on_error           = Color::fromARGB(0xFFFFFFFF);
            t.colors.error_container    = Color::fromARGB(0xFF690003);
            t.colors.on_error_container = Color::fromARGB(0xFFFFDAD6);
            t.colors.success            = Color::fromARGB(0xFF30D158);
            t.colors.on_success         = Color::fromARGB(0xFF000000);
            t.colors.warning            = Color::fromARGB(0xFFFF9F0A);
            t.colors.on_warning         = Color::fromARGB(0xFF000000);
            t.colors.outline            = Color::fromARGB(0xFF38383A);
            t.colors.outline_variant    = Color::fromARGB(0xFF48484A);
            t.colors.shadow             = Color::fromARGB(0xFF000000);
            t.colors.scrim              = Color::fromARGB(0x80000000);
            t.colors.inverse_surface    = Color::fromARGB(0xFFF2F2F7);
            t.colors.inverse_on_surface = Color::fromARGB(0xFF000000);
            t.colors.inverse_primary    = Color::fromARGB(0xFF007AFF);

            t.shape.radius_xs = 6.0f;
            t.shape.radius_sm = 8.0f;
            t.shape.radius_md = 10.0f;
            t.shape.radius_lg = 14.0f;
            t.shape.radius_xl = 20.0f;

            t.elevation = ElevationTokens{0.0f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f};

            t.typography = makeTypography(t.colors.on_surface);
            return t;
        }
    } // namespace

    CupertinoDesignSystem::CupertinoDesignSystem() : tokens_(makeLightTokens()) {}
    CupertinoDesignSystem::CupertinoDesignSystem(DesignTokens tokens, CupertinoMaterial material)
        : tokens_(std::move(tokens)), material_(material) {}

    CupertinoDesignSystem CupertinoDesignSystem::light() { return CupertinoDesignSystem(makeLightTokens()); }
    CupertinoDesignSystem CupertinoDesignSystem::dark()  { return CupertinoDesignSystem(makeDarkTokens()); }

    CupertinoDesignSystem CupertinoDesignSystem::liquidGlass(bool dark)
    {
        return CupertinoDesignSystem(
            dark ? makeDarkTokens() : makeLightTokens(),
            CupertinoMaterial::liquidGlass);
    }

    // -----------------------------------------------------------------------
    // Button — filled/tinted/plain/destructive, 14pt corner (not a stadium)
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildButton(const ButtonConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        Color bg, fg;
        bool has_background = true;

        switch (cfg.priority) {
            case ButtonPriority::secondary:
                // iOS "tinted" button style: a light background tinted with
                // the accent color, accent-colored text.
                bg = withOpacity(c.primary, 0.15f);
                fg = c.primary;
                break;
            case ButtonPriority::tertiary:
                // iOS "plain" button style: text only, no background.
                bg = Color::transparent();
                fg = c.primary;
                has_background = false;
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

        WidgetRef content = cfg.label;
        if (cfg.leading_icon || cfg.trailing_icon) {
            auto row = std::make_shared<Row>();
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            if (cfg.leading_icon) row->children.push_back(cfg.leading_icon);
            row->children.push_back(cfg.label);
            if (cfg.trailing_icon) row->children.push_back(cfg.trailing_icon);
            content = row;
        }

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(20.0f, 10.0f);
        padded->child   = content;

        WidgetRef result = padded;
        if (has_background) {
            BoxDecoration deco;
            deco.color         = bg;
            deco.border_radius = tokens_.shape.radius_lg; // 14pt — not a pill
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = padded;
            result = decorated;
        }

        (void)fg; // label color is caller-provided via cfg.label
        auto detector = std::make_shared<GestureDetector>();
        detector->on_tap = (cfg.enabled && cfg.on_pressed) ? cfg.on_pressed : nullptr;
        detector->child  = result;

        if (!cfg.enabled || !cfg.on_pressed) {
            auto faded = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = detector;
            return faded;
        }
        return detector;
    }

    // -----------------------------------------------------------------------
    // Switch — UISwitch's real default: a *green* active track, thumb
    // stays white in both states.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildSwitch(const SwitchConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sw = std::make_shared<Switch>();
        sw->value = cfg.value;
        sw->active_track_color   = c.success;
        sw->inactive_track_color = c.outline_variant;
        sw->active_thumb_color   = Color::white();
        sw->inactive_thumb_color = Color::white();

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
    // Checkbox — no direct HIG equivalent; approximated as a small tinted
    // rounded square.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildCheckbox(const CheckboxConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto cb = std::make_shared<Checkbox>();
        cb->value = cfg.value;
        cb->active_color  = c.primary;
        cb->check_color   = c.on_primary;
        cb->border_color  = c.outline;
        cb->border_radius = tokens_.shape.radius_sm;

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
    // Radio — no direct HIG equivalent; approximated as a tinted circle.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildRadio(const RadioConfig& cfg) const
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
    // Slider — UISlider's track is thin (4pt), the opposite of MD3's thick
    // 16pt track.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildSlider(const SliderConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto sl = std::make_shared<Slider>();
        sl->value = cfg.value;
        sl->min   = cfg.min;
        sl->max   = cfg.max;
        sl->active_color   = c.primary;
        sl->inactive_color = c.outline_variant;
        sl->track_height   = 4.0f;
        sl->thumb_radius   = 14.0f; // larger white knob, HIG-style

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
    // TextField — UITextField .roundedRect style: filled, no visible
    // border at rest.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildTextField(const TextFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tf = std::make_shared<TextField>();
        tf->placeholder          = cfg.placeholder;
        tf->obscure_text         = cfg.obscure_text;
        tf->max_lines            = cfg.max_lines;
        tf->fill_color           = c.surface_variant;
        tf->border_color         = Color::transparent();
        tf->focused_border_color = c.primary;
        tf->cursor_color         = c.primary;
        tf->selection_color      = withOpacity(c.primary, 0.3f);
        tf->placeholder_color    = c.on_surface_variant;
        tf->border_radius        = tokens_.shape.radius_md;
        tf->border_width         = 1.0f;
        tf->min_height           = 36.0f; // iOS default text field height

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
    // Card — HIG's closest equivalent is an inset grouped section.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildCard(const CardConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto inner = std::make_shared<Padding>();
        inner->padding = cfg.padding;
        inner->child   = cfg.child;

        // Liquid Glass only for the "elevated" (floating) priority — a
        // filled or outlined card is a flat/grouped-list-style surface in
        // real HIG, not a floating glass panel, so those two stay classic
        // even when this DesignSystem is in liquidGlass mode.
        if (material_ == CupertinoMaterial::liquidGlass &&
            cfg.priority == CardPriority::elevated)
        {
            auto bf = std::make_shared<BackdropFilter>();
            bf->filter = ImageFilter::liquidGlass(
                tokens_.shape.radius_lg, withOpacity(c.surface, 0.45f));
            bf->child  = inner;

            // Deliberately no ClipRRect wrapping bf here — confirmed (not
            // just suspected) to break the glass content: Renderer::
            // applyClipShape() renders a ClipRRect's subtree into a
            // separate, small offscreen texture, locally translated so the
            // clip region's top-left becomes (0,0), then runs a *nested*
            // flushDrawList() against that tiny local viewport. A
            // BackdropFilter inside that subtree composites by sampling
            // blurred_backdrop_tex_ — the one full-window backdrop capture
            // — using UVs computed for that nested, locally-offset
            // viewport rather than the card's real screen position, so it
            // shows whatever's near the window's top-left instead of what's
            // actually behind the card. (An earlier, wrong theory attributed
            // the original scroll-artifact bug this omission fixed to
            // OffsetLayer::record() not evicting its own stale GPU replay
            // cache — that bug is real and was fixed separately for the
            // Card's shadow, see TODO.md, but is unrelated to this one; do
            // not re-attempt adding ClipRRect here without first teaching
            // BackdropFilter's compositing to account for an enclosing
            // clip's local viewport offset.) The tradeoff stands as
            // before: caller-supplied child content wide/tall enough to
            // overflow the rounded corners won't get clipped — the shader
            // still self-shapes the glass itself via its own SDF+alpha, so
            // this is a minor cosmetic edge case for realistic card content,
            // not a correctness issue for the glass surface.
            //
            // Same soft shadow as the classic elevated card (transparent
            // fill — the glass shader paints its own surface, this
            // DecoratedBox exists purely for the shadow).
            BoxDecoration shadow_deco;
            shadow_deco.border_radius = tokens_.shape.radius_lg;
            shadow_deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.10f),
                    Offset{0.0f, tokens_.elevation.level1},
                    tokens_.elevation.level1 * 3.0f
                }
            };
            auto shadowed = std::make_shared<DecoratedBox>();
            shadowed->decoration = shadow_deco;
            shadowed->child      = bf;
            return shadowed;
        }

        BoxDecoration deco;
        deco.border_radius = tokens_.shape.radius_lg;

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
                deco.box_shadow = {
                    BoxShadow{
                        Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.08f),
                        Offset{0.0f, tokens_.elevation.level1},
                        tokens_.elevation.level1 * 3.0f
                    }
                };
                break;
        }

        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = inner;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // ProgressIndicator
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildProgressIndicator(const ProgressConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        if (cfg.type == ProgressType::circular) {
            auto pi = std::make_shared<CircularProgressIndicator>();
            if (cfg.value.has_value()) pi->value = *cfg.value;
            pi->value_color      = c.primary;
            pi->background_color = c.outline_variant;
            pi->stroke_width     = 3.0f;
            return pi;
        }
        auto pi = std::make_shared<LinearProgressIndicator>();
        if (cfg.value.has_value()) pi->value = *cfg.value;
        pi->value_color      = c.primary;
        pi->background_color = c.outline_variant;
        pi->min_height       = 4.0f;
        return pi;
    }

    // -----------------------------------------------------------------------
    // Tooltip
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildTooltip(const TooltipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        auto tt = std::make_shared<Tooltip>();
        tt->message          = cfg.message;
        tt->child             = cfg.child;
        tt->text_color       = c.inverse_on_surface;
        tt->border_radius    = tokens_.shape.radius_xs;
        tt->padding             = EdgeInsets::symmetric(8.0f, 4.0f);
        tt->font_size           = 13.0f;
        tt->display_duration_ms = 2500.0;
        if (material_ == CupertinoMaterial::liquidGlass) {
            tt->background_color  = withOpacity(c.inverse_surface, 0.55f);
            tt->backdrop_filter   = ImageFilter::liquidGlass(tokens_.shape.radius_xs,
                                                               withOpacity(c.inverse_surface, 0.55f));
        } else {
            tt->background_color = c.inverse_surface;
        }
        return tt;
    }

    // -----------------------------------------------------------------------
    // ListTile — iOS table-row heights (44/60), shorter than Material's
    // (56/72).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildListTile(const ListTileConfig& cfg) const
    {
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
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(12.0f));
        }
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(text_section)));
        if (cfg.trailing) {
            row_children.push_back(SizedBox::from_width(12.0f));
            row_children.push_back(cfg.trailing);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(16.0f, 8.0f);
        padded->child   = row;

        const float min_h = cfg.subtitle ? 60.0f : 44.0f; // iOS default row heights
        const float inf   = std::numeric_limits<float>::infinity();
        auto constrained = std::make_shared<ConstrainedBox>();
        constrained->additional_constraints = BoxConstraints{0.0f, inf, min_h, inf};
        constrained->child = padded;

        WidgetRef result = constrained;
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
    // Divider
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildDivider(const DividerConfig& cfg) const
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
    // AppBar — flat navigation-bar container, no elevation.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildAppBar(const AppBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> row_children;
        if (cfg.leading) {
            row_children.push_back(cfg.leading);
            row_children.push_back(SizedBox::from_width(12.0f));
        }
        if (cfg.title) {
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(cfg.title)));
        } else {
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(SizedBox::shrink())));
        }
        for (const auto& action : cfg.actions) {
            row_children.push_back(SizedBox::from_width(8.0f));
            row_children.push_back(action);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(16.0f, 10.0f);
        padded->child   = row;

        auto container = std::make_shared<Container>();
        container->color = c.surface;
        container->child = padded;
        return container;
    }

    // -----------------------------------------------------------------------
    // NavigationBar — UITabBar: selected item is a plain tint-color change,
    // NO indicator pill (unlike MD3's tonal pill behind the icon).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildNavigationBar(const NavigationBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;
            Color tint = selected ? c.primary : c.on_surface_variant;

            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::center;
            if (item.icon) col->children.push_back(item.icon);
            if (!item.label.empty()) {
                TextStyle ts;
                ts.font_size = 11.0f; // UITabBarItem label size
                ts.color = tint;
                col->children.push_back(std::make_shared<Text>(item.label, ts));
            }

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(8.0f, 4.0f);
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
    // Dialog — UIAlertController: narrow fixed width (270pt), centered
    // text, action buttons divided by hairlines instead of a right-aligned
    // row (side-by-side for <=2 actions, stacked for 3+ — real alert
    // layout).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildDialog(const DialogConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> children;

        if (cfg.title) {
            // height_factor = 1.0 is load-bearing, not decorative: Align
            // without it fills all available height rather than
            // shrink-wrapping to the child (see this class's buildCard()/
            // buildPrimaryActionButton() history of the same mistake) —
            // here that means it swallows the entire loose vertical budget
            // showDialog()'s Center hands the Column, one Align per title/
            // content, well before the Column ever reaches the actions row.
            auto centered = std::make_shared<Align>(Alignment::center(), cfg.title);
            centered->height_factor = 1.0f;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(16.0f, 20.0f, 16.0f, 4.0f);
            padded->child   = centered;
            children.push_back(padded);
        }
        if (cfg.content) {
            auto centered = std::make_shared<Align>(Alignment::center(), cfg.content);
            centered->height_factor = 1.0f;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(16.0f, 4.0f, 16.0f, 16.0f);
            padded->child   = centered;
            children.push_back(padded);
        }

        if (!cfg.actions.empty()) {
            auto hairline = [&] {
                auto line = std::make_shared<Container>();
                line->height = 1.0f;
                line->color  = c.outline_variant;
                return line;
            };
            auto vhairline = [&] {
                auto line = std::make_shared<Container>();
                line->width = 1.0f;
                line->color = c.outline_variant;
                return line;
            };

            children.push_back(hairline());

            if (cfg.actions.size() <= 2) {
                // Real iOS alerts lay 1-2 actions side by side, separated
                // by a vertical hairline — not a right-aligned button row.
                //
                // Deliberately NOT cross_axis_alignment::stretch (tried
                // first, twice — see TODO.md's Liquid Glass entry): stretch
                // makes a Row report its OWN height as whatever max_height
                // it's handed, and this Row's ancestor chain (the outer
                // Column, sized by showDialog()'s Center, which loosens
                // rather than bounds) hands it a large loose budget, not
                // the buttons' true content height — so a fixed-height
                // wrapper became necessary, and centering the (naturally
                // much shorter) buttons within that artificial slack space
                // is exactly where a small residual text-baseline offset
                // became visible, even after Cupertino/iOS's tightened
                // vertical text metrics. MaterialDesignSystem's own action
                // row (no stretch, no artificial height, buttons simply
                // determine the row's natural height) never shows this,
                // confirming the manufactured slack — not text rendering —
                // was the real cause. cross_axis_alignment::center here
                // matches that: the row sizes to its tallest child (the
                // buttons) with zero artificial slack, so there's nothing
                // for a sub-pixel offset to be visible within.
                std::vector<WidgetRef> row_children;
                for (size_t i = 0; i < cfg.actions.size(); ++i) {
                    if (i > 0) {
                        auto divider = vhairline();
                        // No natural height of its own (a bare 1px-wide
                        // Container with unset height reports 0 under the
                        // loose constraints this row hands non-flex
                        // children) — an explicit height is required for
                        // it to be visible at all now that stretch isn't
                        // doing that job. Approximates real button content
                        // height; exact dynamic matching isn't possible
                        // here since cfg.actions[i] is a caller-supplied,
                        // opaque WidgetRef whose true height buildDialog()
                        // has no way to query ahead of layout.
                        divider->height = 24.0f;
                        row_children.push_back(divider);
                    }
                    // buildButton() sizes to its own label content and
                    // doesn't center within extra width handed to it, so a
                    // bare Expanded button reads left-aligned inside its
                    // half. Center fixes that, but height_factor = 1.0 is
                    // required — without it, Align/Center reports
                    // constraints_.max_height when no factor is set, and
                    // this Expanded child's height constraint is loose up
                    // to the Row's own max_cross, which (see the row-level
                    // comment above) is inherited from the same large loose
                    // ancestor budget this whole rewrite exists to avoid.
                    // height_factor = 1.0 makes it shrink-wrap to the
                    // button's natural height instead, so it can't inflate
                    // the row's cross_size the way a factor-less Center did
                    // when this was still wrapped in the old stretch/fixed-
                    // height composition.
                    auto centered_action = std::make_shared<Center>(cfg.actions[i]);
                    centered_action->height_factor = 1.0f;
                    row_children.push_back(std::make_shared<Expanded>(WidgetRef(centered_action)));
                }
                auto row = std::make_shared<Row>();
                row->cross_axis_alignment = CrossAxisAlignment::center;
                row->children = std::move(row_children);
                children.push_back(row);
            } else {
                // 3+ actions stack vertically, each divided by a hairline.
                std::vector<WidgetRef> col_children;
                for (size_t i = 0; i < cfg.actions.size(); ++i) {
                    if (i > 0) col_children.push_back(hairline());
                    // Same left-alignment issue as the <=2-actions Row
                    // above, fixed the same way — Center's content within
                    // the width the Column's own stretch already forces
                    // tight. Unlike that Row case, though, height here is
                    // loose (this Column's non-flex/non-Expanded children
                    // get {0, remaining_main} — see RenderFlex's first
                    // pass), so height_factor = 1.0 stays required to
                    // avoid the exact "Align fills all available height"
                    // hazard fixed above for the title/content wrappers.
                    auto centered_action = std::make_shared<Center>(cfg.actions[i]);
                    centered_action->height_factor = 1.0f;
                    col_children.push_back(centered_action);
                }
                auto actions_col = std::make_shared<Column>();
                actions_col->main_axis_size = MainAxisSize::min;
                actions_col->cross_axis_alignment = CrossAxisAlignment::stretch;
                actions_col->children = std::move(col_children);
                children.push_back(actions_col);
            }
        } else {
            children.push_back(SizedBox::from_height(16.0f));
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(children);

        auto dialog = std::make_shared<Dialog>();
        dialog->child             = col;
        dialog->border_radius     = tokens_.shape.radius_lg;
        dialog->elevation         = tokens_.elevation.level2;
        dialog->min_width         = 270.0f; // UIAlertController's fixed width
        dialog->max_width         = 270.0f;
        if (material_ == CupertinoMaterial::liquidGlass) {
            dialog->background_color  = withOpacity(c.surface, 0.45f);
            dialog->backdrop_filter   = ImageFilter::liquidGlass(tokens_.shape.radius_lg,
                                                                   withOpacity(c.surface, 0.45f));
        } else {
            dialog->background_color = c.surface;
        }
        return dialog;
    }

    // -----------------------------------------------------------------------
    // SnackBar — no direct HIG equivalent; approximated as a dark toast.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildSnackBar(const SnackBarConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        TextStyle msg_style;
        msg_style.font_size = 13.0f;
        msg_style.color = c.inverse_on_surface;
        auto msg_text = std::make_shared<Text>(cfg.message, msg_style);

        std::vector<WidgetRef> row_children;
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(msg_text)));

        if (cfg.action_label.has_value() && cfg.on_action) {
            TextStyle action_style;
            action_style.font_size = 13.0f;
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
        sb->border_radius    = tokens_.shape.radius_lg;
        sb->padding          = EdgeInsets::symmetric(16.0f, 12.0f);
        sb->duration_ms      = cfg.duration_ms;
        return sb;
    }

    // -----------------------------------------------------------------------
    // PopupMenuButton — UIMenu-style rounded popup.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildPopupMenuButton(const PopupMenuConfig& cfg) const
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
                ts.font_size = 15.0f;
                ts.color = c.on_surface;
                pmi.child = std::make_shared<Text>(item.label, ts);
            }
            items.push_back(std::move(pmi));
        }

        auto pmb = std::make_shared<PopupMenuButton>();
        pmb->items         = std::move(items);
        pmb->on_selected   = cfg.on_selected;
        pmb->child         = cfg.child;
        pmb->border_radius = tokens_.shape.radius_lg;
        pmb->elevation     = tokens_.elevation.level2;
        if (material_ == CupertinoMaterial::liquidGlass) {
            pmb->popup_color      = withOpacity(c.surface, 0.45f);
            pmb->backdrop_filter  = ImageFilter::liquidGlass(tokens_.shape.radius_lg,
                                                               withOpacity(c.surface, 0.45f));
        } else {
            pmb->popup_color = c.surface_variant;
        }
        return pmb;
    }

    // -----------------------------------------------------------------------
    // DropdownButton
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildDropdownButton(const DropdownConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<DropdownMenuItem<std::string>> items;
        for (const auto& item : cfg.items) {
            DropdownMenuItem<std::string> dmi;
            dmi.value = item.value;
            dmi.enabled = true;
            TextStyle ts;
            ts.font_size = 15.0f;
            ts.color = c.on_surface;
            dmi.child = std::make_shared<Text>(item.label, ts);
            items.push_back(std::move(dmi));
        }

        auto dd = std::make_shared<DropdownButton<std::string>>();
        dd->items = std::move(items);
        dd->hint  = cfg.hint;
        if (cfg.selected_value.has_value()) dd->value = *cfg.selected_value;
        if (cfg.on_changed) dd->on_changed = cfg.on_changed;
        dd->border_radius  = tokens_.shape.radius_lg;
        dd->elevation      = tokens_.elevation.level2;
        if (material_ == CupertinoMaterial::liquidGlass) {
            dd->dropdown_color  = withOpacity(c.surface, 0.45f);
            dd->backdrop_filter = ImageFilter::liquidGlass(tokens_.shape.radius_lg,
                                                             withOpacity(c.surface, 0.45f));
        } else {
            dd->dropdown_color = c.surface_variant;
        }
        return dd;
    }

    // -----------------------------------------------------------------------
    // PrimaryActionButton (FAB) — HIG has no equivalent; falls back to a
    // plain circular button, flat (no heavy Material-style shadow).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildPrimaryActionButton(const PrimaryActionConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        const float diameter = 56.0f;
        const float el       = tokens_.elevation.level2;

        WidgetRef content;
        if (cfg.icon) {
            content = cfg.icon;
        } else if (cfg.label) {
            content = cfg.label;
        } else {
            content = std::make_shared<Text>("+",
                TextStyle{}.withFontSize(24.0f).withColor(
                    material_ == CupertinoMaterial::liquidGlass ? c.primary : c.on_primary));
        }

        // Align must be the *inner* widget, SizedBox outermost — see
        // campello_ui's buildPrimaryActionButton() for the full
        // explanation of why the reverse nesting silently expands to fill
        // available space instead of staying pinned to 56x56.
        auto centered = std::make_shared<Align>(Alignment::center(), content);
        auto sized    = SizedBox::square(diameter, centered);

        BoxDecoration shadow_deco;
        shadow_deco.border_radius = tokens_.shape.radius_full;
        shadow_deco.box_shadow = {
            BoxShadow{
                Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.10f),
                Offset{0.0f, el},
                el * 2.0f
            }
        };

        WidgetRef decorated;
        if (material_ == CupertinoMaterial::liquidGlass) {
            // corner_radius == half the box size degenerates the shader's
            // rounded-rect SDF into a perfect circle — see TODO.md's
            // Liquid Glass entry. Icon/label content is small and
            // centered, so it stays within the circle without needing an
            // extra ClipOval.
            auto bf = std::make_shared<BackdropFilter>();
            bf->filter = ImageFilter::liquidGlass(diameter * 0.5f, withOpacity(c.primary, 0.55f));
            bf->child  = sized;

            shadow_deco.color = Color::transparent();
            auto shadowed = std::make_shared<DecoratedBox>();
            shadowed->decoration = shadow_deco;
            shadowed->child      = bf;
            decorated = shadowed;
        } else {
            BoxDecoration deco  = shadow_deco;
            deco.color          = c.primary;
            auto solid = std::make_shared<DecoratedBox>();
            solid->decoration = deco;
            solid->child      = sized;
            decorated = solid;
        }

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
    // TabBar — thinner (2pt) indicator than MD3's 3pt.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildTabBar(const TabBarConfig& cfg) const
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
        tb->indicator_weight       = 2.0f;
        tb->background_color       = c.surface;
        return tb;
    }

    // -----------------------------------------------------------------------
    // Chip — no direct HIG equivalent; a pill (fully rounded), matching
    // iOS's general fondness for capsule UI (filter pills, mode selectors).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildChip(const ChipConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        Color bg = cfg.selected ? c.primary_container : c.surface_variant;
        Color fg = cfg.selected ? c.on_primary_container : c.on_surface_variant;

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
        deco.border_radius = tokens_.shape.radius_full;

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
    // SegmentedButton — UISegmentedControl's real look: a gray pill track
    // with a white/surface pill inset behind the selected segment. This is
    // the authentic home for this pattern (TabBar's underline model can't
    // represent it).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildSegmentedButton(const SegmentedConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> row_children;
        for (size_t i = 0; i < cfg.segments.size(); ++i) {
            const auto& seg = cfg.segments[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;

            std::vector<WidgetRef> content_children;
            if (seg.icon) content_children.push_back(seg.icon);
            if (seg.label) content_children.push_back(seg.label);
            auto content_row = std::make_shared<Row>();
            content_row->main_axis_alignment = MainAxisAlignment::center;
            content_row->main_axis_size = MainAxisSize::min;
            content_row->children = std::move(content_children);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(10.0f, 6.0f);
            padded->child   = content_row;

            WidgetRef segment_widget = padded;
            if (selected) {
                // The inset white pill, floated above the gray track.
                BoxDecoration inner;
                inner.color         = c.surface;
                inner.border_radius = tokens_.shape.radius_full;
                inner.box_shadow = {
                    BoxShadow{
                        Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.10f),
                        Offset{0.0f, tokens_.elevation.level1},
                        tokens_.elevation.level1 * 2.0f
                    }
                };
                auto decorated = std::make_shared<DecoratedBox>();
                decorated->decoration = inner;
                decorated->child      = padded;
                segment_widget = decorated;
            }

            auto inset = std::make_shared<Padding>();
            inset->padding = EdgeInsets::all(2.0f); // gap between segments and the track edge
            inset->child   = segment_widget;

            WidgetRef item = inset;
            if (cfg.on_changed && cfg.enabled) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_changed, idx = static_cast<int>(i)]() { cb(idx); };
                gesture->child  = inset;
                item = gesture;
            }
            row_children.push_back(std::make_shared<Expanded>(WidgetRef(item)));
        }

        auto container_row = std::make_shared<Row>();
        container_row->cross_axis_alignment = CrossAxisAlignment::stretch;
        container_row->children = std::move(row_children);

        BoxDecoration outer;
        outer.color         = c.surface_variant; // the gray track
        outer.border_radius = tokens_.shape.radius_full;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = container_row;

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
    // BottomSheet — iOS sheet presentation: 20pt top corners, a grabber.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildBottomSheet(const BottomSheetConfig& cfg) const
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
            sized_handle->width  = 36.0f;
            sized_handle->height = 5.0f;
            sized_handle->child  = handle_decorated;

            auto padded_handle = std::make_shared<Padding>();
            padded_handle->padding = EdgeInsets::symmetric(0.0f, 8.0f);
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
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    // -----------------------------------------------------------------------
    // Badge — with a thin surface-colored ring, matching Apple's badge
    // treatment for legibility against whatever it overlaps.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildBadge(const BadgeConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        WidgetRef badge_content;
        float diameter = 10.0f;
        if (cfg.label.has_value()) {
            diameter = 16.0f;
            TextStyle ts;
            ts.font_size = 10.0f;
            ts.color = Color::white();
            badge_content = std::make_shared<Text>(*cfg.label, ts);
        }

        BoxDecoration inner_deco;
        inner_deco.color         = c.error;
        inner_deco.border_radius = tokens_.shape.radius_full;
        auto inner = std::make_shared<DecoratedBox>();
        inner->decoration = inner_deco;
        if (badge_content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(4.0f, 2.0f);
            padded->child   = badge_content;
            inner->child = padded;
        }

        auto sized_inner = std::make_shared<SizedBox>();
        if (!cfg.label.has_value()) {
            sized_inner->width  = diameter;
            sized_inner->height = diameter;
        }
        sized_inner->child = inner;

        // The ring: a slightly larger surface-colored circle behind the
        // badge, separating it visually from whatever it overlaps.
        BoxDecoration ring_deco;
        ring_deco.color         = c.surface;
        ring_deco.border_radius = tokens_.shape.radius_full;
        auto ring = std::make_shared<DecoratedBox>();
        ring->decoration = ring_deco;
        auto ring_padding = std::make_shared<Padding>();
        ring_padding->padding = EdgeInsets::all(1.5f);
        ring_padding->child   = sized_inner;
        ring->child = ring_padding;

        auto positioned = std::make_shared<Positioned>();
        positioned->right = -4.0f;
        positioned->top   = -4.0f;
        positioned->child = ring;

        auto stack = std::make_shared<Stack>();
        stack->children = {cfg.child, positioned};
        return stack;
    }

    // -----------------------------------------------------------------------
    // IconButton — plain at rest, tinted circle when selected (matching
    // the button "tinted" style established above).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildIconButton(const IconButtonConfig& cfg) const
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
    // Stepper — UIStepper's real look: a single connected two-segment
    // control ([-][+]), no built-in value label (the real control has
    // none; apps display the value in their own separate label).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildStepper(const StepperConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        const bool can_dec = cfg.enabled && cfg.on_changed && cfg.value > cfg.min;
        const bool can_inc = cfg.enabled && cfg.on_changed && cfg.value < cfg.max;

        auto makeSegment = [&](const std::string& glyph, std::function<void()> on_tap) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(14.0f, 6.0f);
            padded->child   = std::make_shared<Text>(glyph,
                TextStyle{}.withFontSize(16.0f).withColor(c.primary));
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = std::move(on_tap);
            gesture->child  = padded;
            return gesture;
        };

        auto dec = makeSegment("-", can_dec
            ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, lo = cfg.min] {
                  cb(std::max(lo, v - step));
              })
            : nullptr);
        auto inc = makeSegment("+", can_inc
            ? std::function<void()>([cb = cfg.on_changed, v = cfg.value, step = cfg.step, hi = cfg.max] {
                  cb(std::min(hi, v + step));
              })
            : nullptr);

        auto divider = std::make_shared<Container>();
        divider->width = 1.0f;
        divider->color = c.outline;

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::stretch;
        row->main_axis_size = MainAxisSize::min;
        row->children = {dec, divider, inc};

        BoxDecoration deco;
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

    // -----------------------------------------------------------------------
    // RatingIndicator — filled stars use `warning` (Apple's real App Store
    // rating stars are golden/orange, not the app's blue accent).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildRatingIndicator(const RatingConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> stars;
        for (int i = 0; i < cfg.max; ++i) {
            bool filled = i < cfg.value;
            auto glyph = std::make_shared<Text>(filled ? "*" : "-",
                TextStyle{}.withFontSize(18.0f).withColor(filled ? c.warning : c.outline));
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
    // ActionSheet — the classic iOS pattern: a rounded list card, a visible
    // gap, then a *separate* rounded card holding just Cancel. Real
    // UIActionSheet/UIAlertController(.actionSheet) layout.
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildActionSheet(const ActionSheetConfig& cfg) const
    {
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> actions_children;

        if (cfg.title) {
            // height_factor = 1.0 is load-bearing, not decorative — see
            // buildDialog()'s title/content wrappers for why an Align
            // without it fills all available height instead of
            // shrink-wrapping to the child (TODO.md's Liquid Glass entry
            // has the full history of that bug).
            auto centered = std::make_shared<Align>(Alignment::center(), cfg.title);
            centered->height_factor = 1.0f;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::all(16.0f);
            padded->child   = centered;
            actions_children.push_back(padded);
            auto hairline = std::make_shared<Container>();
            hairline->height = 1.0f;
            hairline->color  = c.outline_variant;
            actions_children.push_back(hairline);
        }

        for (size_t i = 0; i < cfg.actions.size(); ++i) {
            const auto& action = cfg.actions[i];
            TextStyle ts;
            ts.font_size = 17.0f;
            ts.color = action.destructive ? c.error : c.primary;
            auto text = std::make_shared<Text>(action.label, ts);
            auto centered = std::make_shared<Align>(Alignment::center(), text);
            centered->height_factor = 1.0f;
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(16.0f, 14.0f);
            padded->child   = centered;
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = action.on_selected;
            gesture->child  = padded;
            actions_children.push_back(gesture);

            if (i + 1 < cfg.actions.size()) {
                auto hairline = std::make_shared<Container>();
                hairline->height = 1.0f;
                hairline->color  = c.outline_variant;
                actions_children.push_back(hairline);
            }
        }

        auto actions_col = std::make_shared<Column>();
        actions_col->main_axis_size = MainAxisSize::min;
        actions_col->cross_axis_alignment = CrossAxisAlignment::stretch;
        actions_col->children = std::move(actions_children);

        // Real iOS action sheets are floating glass panels in Liquid Glass
        // mode. Shared by both the actions card and the separate Cancel
        // card below — same shadow-DecoratedBox-wrapping-BackdropFilter
        // composition as buildCard()/buildDialog() (no ClipRRect; see
        // TODO.md's Liquid Glass entry for why that's a confirmed
        // incompatibility, not an oversight).
        auto makeSheetCard = [&](WidgetRef content) -> WidgetRef {
            if (material_ == CupertinoMaterial::liquidGlass) {
                auto bf = std::make_shared<BackdropFilter>();
                bf->filter = ImageFilter::liquidGlass(tokens_.shape.radius_lg,
                                                        withOpacity(c.surface, 0.45f));
                bf->child  = std::move(content);
                BoxDecoration shadow_deco;
                shadow_deco.border_radius = tokens_.shape.radius_lg;
                auto shadowed = std::make_shared<DecoratedBox>();
                shadowed->decoration = shadow_deco;
                shadowed->child      = bf;
                return shadowed;
            }
            BoxDecoration deco;
            deco.color         = c.surface;
            deco.border_radius = tokens_.shape.radius_lg;
            auto decorated = std::make_shared<DecoratedBox>();
            decorated->decoration = deco;
            decorated->child      = std::move(content);
            return decorated;
        };

        auto actions_card = makeSheetCard(actions_col);

        if (!cfg.on_cancel) return actions_card;

        TextStyle cancel_ts;
        cancel_ts.font_size = 17.0f;
        cancel_ts.color = c.primary;
        cancel_ts.font_weight = FontWeight::bold;
        auto cancel_text = std::make_shared<Text>("Cancel", cancel_ts);
        auto cancel_centered = std::make_shared<Align>(Alignment::center(), cancel_text);
        cancel_centered->height_factor = 1.0f;
        auto cancel_padded = std::make_shared<Padding>();
        cancel_padded->padding = EdgeInsets::symmetric(16.0f, 14.0f);
        cancel_padded->child   = cancel_centered;
        auto cancel_gesture = std::make_shared<GestureDetector>();
        cancel_gesture->on_tap = cfg.on_cancel;
        cancel_gesture->child  = cancel_padded;

        auto cancel_card = makeSheetCard(cancel_gesture);

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = {actions_card, SizedBox::from_height(8.0f), cancel_card};
        return col;
    }

    // -----------------------------------------------------------------------
    // SearchField — UISearchBar: rounded rect (not a pill, unlike MD3).
    // -----------------------------------------------------------------------

    WidgetRef CupertinoDesignSystem::buildSearchField(const SearchFieldConfig& cfg) const
    {
        const auto& c = tokens_.colors;

        auto icon = std::make_shared<Text>("search",
            TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));

        auto controller = std::make_shared<TextEditingController>(cfg.value);
        auto tf = std::make_shared<TextField>(controller, cfg.placeholder);
        tf->fill_color        = Color::transparent();
        tf->border_color      = Color::transparent();
        tf->placeholder_color = c.on_surface_variant;
        tf->min_height        = 36.0f;
        if (cfg.on_changed)
            tf->on_changed = [cb = cfg.on_changed](const std::string& v) { cb(v); };

        std::vector<WidgetRef> row_children;
        row_children.push_back(icon);
        row_children.push_back(SizedBox::from_width(6.0f));
        row_children.push_back(std::make_shared<Expanded>(WidgetRef(tf)));
        if (!cfg.value.empty() && cfg.on_clear) {
            auto clear = std::make_shared<Text>("x",
                TextStyle{}.withFontSize(12.0f).withColor(c.on_surface_variant));
            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = cfg.on_clear;
            gesture->child  = clear;
            row_children.push_back(SizedBox::from_width(6.0f));
            row_children.push_back(gesture);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(10.0f, 4.0f);
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = c.surface_variant;
        deco.border_radius = tokens_.shape.radius_md; // rounded rect, not a pill
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
    // DatePicker / TimePicker — the common iOS list-row convention: no box
    // chrome at all, value shown inline in accent-colored text (unlike
    // Material's outlined field). Chrome only, see DatePickerConfig's doc.
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
                TextStyle{}.withFontSize(15.0f).withColor(c.primary));

            auto row = std::make_shared<Row>();
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->main_axis_size = MainAxisSize::min;
            row->children = {icon_text, SizedBox::from_width(6.0f), label_text};

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(4.0f, 6.0f);
            padded->child   = row;

            auto gesture = std::make_shared<GestureDetector>();
            gesture->on_tap = (enabled && on_tap) ? std::move(on_tap) : nullptr;
            gesture->child  = padded;

            if (!enabled) {
                auto faded = std::make_shared<Opacity>();
                faded->opacity = 0.4f;
                faded->child   = gesture;
                return faded;
            }
            return gesture;
        }
    } // namespace

    WidgetRef CupertinoDesignSystem::buildDatePicker(const DatePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "date", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef CupertinoDesignSystem::buildTimePicker(const TimePickerConfig& cfg) const
    {
        return buildTriggerField(tokens_, "time", cfg.label, cfg.on_tap, cfg.enabled);
    }

    WidgetRef CupertinoDesignSystem::buildExpansionTile(const ExpansionTileConfig& cfg) const
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
        // iOS disclosure indicator rotates 90deg when expanded; approximated
        // with a glyph swap (no rotation transform wired through this config).
        header_children.push_back(std::make_shared<Text>(cfg.expanded ? "v" : ">",
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
        auto hairline = std::make_shared<Container>();
        hairline->height = 1.0f;
        hairline->color  = c.outline_variant;
        children.push_back(hairline);
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

    WidgetRef CupertinoDesignSystem::buildToggleButtons(const ToggleButtonsConfig& cfg) const
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

            // iOS bordered-tinted button group: selected gets a filled tint
            // background (systemBlue), matching UIButton's .tinted style.
            BoxDecoration deco;
            deco.color         = item.selected ? c.primary : Color::transparent();
            deco.border_radius = tokens_.shape.radius_sm;
            deco.border        = BoxBorder::all(c.primary, 1.0f);
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

    WidgetRef CupertinoDesignSystem::buildBanner(const BannerConfig& cfg) const
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
        padded->padding = EdgeInsets::all(14.0f);
        padded->child   = row;

        std::vector<WidgetRef> col_children = {padded};
        auto hairline = std::make_shared<Container>();
        hairline->height = 1.0f;
        hairline->color  = c.outline_variant;
        col_children.push_back(hairline);

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = CrossAxisAlignment::stretch;
        col->children = std::move(col_children);

        // iOS in-app banners sit on secondarySystemBackground, distinguished
        // by a hairline rather than a heavy tint or shadow.
        BoxDecoration deco;
        deco.color = c.surface_variant;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = col;
        return decorated;
    }

    WidgetRef CupertinoDesignSystem::buildNavigationRail(const NavigationRailConfig& cfg) const
    {
        // iOS/iPadOS has no direct NavigationRail equivalent (its closest
        // relative is the split-view sidebar, which is a whole navigation
        // paradigm, not a single component) — falls back to a compact
        // icon+label column tinted the same way as buildNavigationBar's
        // tab items, no pill indicator (HIG doesn't use one here either).
        const auto& c = tokens_.colors;
        std::vector<WidgetRef> item_widgets;
        for (size_t i = 0; i < cfg.items.size(); ++i) {
            const auto& item = cfg.items[i];
            bool selected = static_cast<int>(i) == cfg.selected_index;
            Color tint = selected ? c.primary : c.on_surface_variant;

            std::vector<WidgetRef> content;
            if (item.icon) content.push_back(item.icon);
            if (cfg.extended && !item.label.empty()) {
                content.push_back(SizedBox::from_width(10.0f));
                content.push_back(std::make_shared<Text>(item.label,
                    TextStyle{}.withFontSize(13.0f).withColor(tint)));
            }

            auto row = std::make_shared<Row>();
            row->main_axis_alignment = cfg.extended ? MainAxisAlignment::start : MainAxisAlignment::center;
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->children = std::move(content);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(cfg.extended ? 16.0f : 8.0f, 10.0f);
            padded->child   = row;

            WidgetRef entry = padded;
            if (cfg.on_tap) {
                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [cb = cfg.on_tap, idx = static_cast<int>(i)] { cb(idx); };
                gesture->child  = padded;
                entry = gesture;
            }
            item_widgets.push_back(entry);
        }

        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->cross_axis_alignment = cfg.extended ? CrossAxisAlignment::stretch : CrossAxisAlignment::center;
        col->children = std::move(item_widgets);

        auto padded_col = std::make_shared<Padding>();
        padded_col->padding = EdgeInsets::symmetric(0.0f, 12.0f);
        padded_col->child   = col;

        BoxDecoration outer;
        outer.color = c.surface_variant;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = padded_col;
        return decorated;
    }

    WidgetRef CupertinoDesignSystem::buildDataTable(const DataTableConfig& cfg) const
    {
        // iOS has no native data-table component (grouped UITableView is
        // the closest analogue, but it's row-of-cells not columnar) — this
        // renders as a plain grouped-list-style table with hairline row
        // dividers, matching the inset-grouped list convention elsewhere
        // (buildListTile / buildActionSheet) rather than Material's boxed
        // outline.
        const auto& c = tokens_.colors;

        auto makeCell = [&](WidgetRef content) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(14.0f, 10.0f);
            padded->child   = std::move(content);
            return std::make_shared<Expanded>(WidgetRef(padded));
        };

        std::vector<WidgetRef> header_cells;
        for (const auto& col_name : cfg.columns) {
            TextStyle ts;
            ts.font_size = 12.0f;
            ts.color = c.on_surface_variant;
            header_cells.push_back(makeCell(std::make_shared<Text>(col_name, ts)));
        }
        auto header_row = std::make_shared<Row>();
        header_row->children = std::move(header_cells);

        std::vector<WidgetRef> table_children = {header_row};
        for (const auto& data_row : cfg.rows) {
            auto hairline = std::make_shared<Container>();
            hairline->height = 1.0f;
            hairline->color  = c.outline_variant;
            table_children.push_back(hairline);

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
        outer.color         = c.surface;
        outer.border_radius = tokens_.shape.radius_md;
        auto decorated = std::make_shared<DecoratedBox>();
        decorated->decoration = outer;
        decorated->child      = col;
        return decorated;
    }

} // namespace systems::leal::campello_widgets
