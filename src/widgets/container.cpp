#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <algorithm>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/clip_rect.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/limited_box.hpp>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // Mirrors Flutter's BoxConstraints.tighten(): width/height are
        // clamped into the existing constraints' own [min, max] range
        // *before* becoming the new tight min==max, rather than overwriting
        // it outright -- otherwise an explicit width/height outside the
        // caller's own `constraints` would silently widen or violate it
        // (e.g. constraints={min:50,max:100} + width:200 must clamp to 100,
        // not force min=max=200).
        std::optional<BoxConstraints> effectiveConstraints(
            std::optional<float> width,
            std::optional<float> height,
            std::optional<BoxConstraints> constraints)
        {
            if (constraints)
            {
                BoxConstraints c = *constraints;
                if (width)
                {
                    const float w = std::clamp(*width, c.min_width, c.max_width);
                    c.min_width = w;
                    c.max_width = w;
                }
                if (height)
                {
                    const float h = std::clamp(*height, c.min_height, c.max_height);
                    c.min_height = h;
                    c.max_height = h;
                }
                return c;
            }
            if (width || height)
                return BoxConstraints::tightFor(width, height);
            return std::nullopt;
        }
    }

    WidgetRef Container::build(BuildContext&) const
    {
        WidgetRef result = child;

        const std::optional<BoxConstraints> effective =
            effectiveConstraints(width, height, constraints);

        // No child and not already tightly sized: synthesize a filler that
        // fills available space when the incoming constraints are bounded,
        // but collapses to zero (rather than reporting an infinite size)
        // when they aren't -- mirrors Flutter's identical
        // `LimitedBox(maxWidth: 0, maxHeight: 0, child: ConstrainedBox(
        // constraints: BoxConstraints.expand()))` trick in Container.build().
        // Without this, `Container{.color = ...}` with no child/size inside
        // an unbounded parent (a Row/Column with no Expanded, say) would hit
        // RenderColoredBox/RenderDecoratedBox's own null-child fallback and
        // report an infinite size.
        if (!result && (!effective || !effective->isTight()))
        {
            auto expand = std::make_shared<ConstrainedBox>();
            expand->additional_constraints = BoxConstraints::expand();

            auto limited        = std::make_shared<LimitedBox>();
            limited->max_width  = 0.0f;
            limited->max_height = 0.0f;
            limited->child      = std::move(expand);

            result = std::move(limited);
        }

        // Composition order (child-outward) mirrors Flutter's Container.build():
        // alignment -> padding -> color/decoration -> foreground_decoration ->
        // constraints -> margin -> transform.

        if (alignment)
        {
            auto w       = std::make_shared<Align>();
            w->alignment = *alignment;
            w->child     = std::move(result);
            result       = std::move(w);
        }

        // Effective padding includes the border's own width, so a full-bleed
        // child doesn't paint underneath the border stroke -- mirrors
        // Flutter's Container._paddingIncludingDecoration (there sourced from
        // Border.dimensions; this codebase's BoxBorder is always uniform, so
        // it's just `border.width` added to all four sides).
        std::optional<EdgeInsets> effective_padding = padding;
        if (decoration && decoration->border)
        {
            const float bw = decoration->border->width;
            effective_padding = padding
                ? EdgeInsets::only(padding->left + bw, padding->top + bw,
                                    padding->right + bw, padding->bottom + bw)
                : EdgeInsets::all(bw);
        }
        if (effective_padding)
        {
            auto w     = std::make_shared<Padding>();
            w->padding = *effective_padding;
            w->child   = std::move(result);
            result     = std::move(w);
        }

        if (decoration)
        {
            // clip_behavior clips the (already aligned/padded) child to the
            // decoration's shape before the decoration itself is painted
            // around it -- matches Flutter's Container (clipBehavior requires
            // decoration or color; see debugValidate()).
            if (clip_behavior != Clip::none)
            {
                if (decoration->border_radius > 0.0f)
                {
                    auto w         = std::make_shared<ClipRRect>();
                    w->border_radius = decoration->border_radius;
                    w->child       = std::move(result);
                    result         = std::move(w);
                }
                else
                {
                    auto w   = std::make_shared<ClipRect>();
                    w->child = std::move(result);
                    result   = std::move(w);
                }
            }

            auto w        = std::make_shared<DecoratedBox>();
            w->decoration = *decoration;
            w->child      = std::move(result);
            result        = std::move(w);
        }
        else if (color)
        {
            // Plain color has no radius/shape of its own -- clipping it can
            // only ever mean a rectangular clip of an overflowing child.
            if (clip_behavior != Clip::none)
            {
                auto w   = std::make_shared<ClipRect>();
                w->child = std::move(result);
                result   = std::move(w);
            }

            auto w   = std::make_shared<ColoredBox>();
            w->color = *color;
            w->child = std::move(result);
            result   = std::move(w);
        }

        if (foreground_decoration)
        {
            auto w        = std::make_shared<DecoratedBox>();
            w->decoration = *foreground_decoration;
            w->position   = DecorationPosition::foreground;
            w->child      = std::move(result);
            result        = std::move(w);
        }

        if (effective)
        {
            auto w                     = std::make_shared<ConstrainedBox>();
            w->additional_constraints  = *effective;
            w->child                   = std::move(result);
            result                     = std::move(w);
        }

        if (margin)
        {
            auto w    = std::make_shared<Padding>();
            w->padding = *margin;
            w->child  = std::move(result);
            result    = std::move(w);
        }

        if (transform)
        {
            // Flutter's Container applies `transform` with no pivot offset
            // unless `transformAlignment` is given -- Alignment::topLeft()
            // resolves to a zero pivot offset (see Alignment::inscribe),
            // which is exactly that "no adjustment" behaviour, unlike
            // Transform's own default of a center pivot.
            auto w        = std::make_shared<Transform>();
            w->transform  = *transform;
            w->alignment  = transform_alignment.value_or(Alignment::topLeft());
            w->child      = std::move(result);
            result        = std::move(w);
        }

        if (!result)
        {
            auto w = std::make_shared<SizedBox>();
            result = std::move(w);
        }

        return result;
    }


    void Container::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        if (width.has_value())
            properties.add(std::make_unique<DoubleProperty>("width", *width));
        if (height.has_value())
            properties.add(std::make_unique<DoubleProperty>("height", *height));
        if (constraints.has_value())
            properties.add(std::make_unique<StringProperty>("constraints", "BoxConstraints(...)"));
        if (color.has_value())
            properties.add(std::make_unique<ColorProperty>("color", *color));
        if (decoration.has_value())
            properties.add(std::make_unique<StringProperty>("decoration", "BoxDecoration(...)"));
        if (foreground_decoration.has_value())
            properties.add(std::make_unique<StringProperty>("foregroundDecoration", "BoxDecoration(...)"));
        if (padding.has_value())
            properties.add(std::make_unique<DiagnosticProperty<EdgeInsets>>("padding", *padding));
        if (margin.has_value())
            properties.add(std::make_unique<DiagnosticProperty<EdgeInsets>>("margin", *margin));
        if (alignment.has_value())
            properties.add(std::make_unique<StringProperty>("alignment", "Alignment(...)"));
        if (transform.has_value())
            properties.add(std::make_unique<StringProperty>("transform", "Matrix4(...)"));
        if (transform_alignment.has_value())
            properties.add(std::make_unique<StringProperty>("transformAlignment", "Alignment(...)"));
        if (clip_behavior != Clip::none)
            properties.add(std::make_unique<StringProperty>("clipBehavior",
                clip_behavior == Clip::hardEdge ? "hardEdge" : "antiAlias"));
    }
} // namespace systems::leal::campello_widgets
