#pragma once

#include <optional>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/widgets/transform.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief How a `Container` clips its child to its `decoration`'s shape.
     *
     * `hardEdge` and `antiAlias` are both implemented as an antialiased
     * rounded-rect/rect clip (this engine has no separate hard-edge clip
     * primitive); the distinction is kept for API familiarity with Flutter.
     */
    enum class Clip
    {
        none,       ///< No clipping (default).
        hardEdge,
        antiAlias,
    };

    /**
     * @brief A convenience widget that combines sizing, padding, decoration,
     * and alignment.
     *
     * Container composes `Align`, `Padding`, `DecoratedBox` (or `ColoredBox`),
     * a second `DecoratedBox` for `foreground_decoration`, `ConstrainedBox`,
     * and `Padding` (for margin) as needed based on the properties that are
     * set, in the same order Flutter's `Container` does (child-outward):
     * `alignment` → `padding` (plus the border's own width, if any, so a
     * full-bleed child doesn't paint under the border stroke) →
     * `color`/`decoration` → `foreground_decoration` →
     * `width`/`height`/`constraints` → `margin` → `transform`. If no
     * properties are set it is equivalent to a transparent box that fills
     * available space.
     *
     * Exactly one of `color` or `decoration` may be set — like Flutter,
     * setting both is a debug-time error since `decoration` already carries
     * its own `color`. `foreground_decoration` is independent of both and
     * paints on top of the child (e.g. a gradient overlay or vignette).
     *
     * When there's no `child` and no already-tight size (from
     * `width`/`height`/`constraints`), `build()` synthesizes a
     * `LimitedBox{0, 0, ConstrainedBox{BoxConstraints::expand()}}` filler
     * as the base child — matching Flutter's identical trick — so the
     * Container fills available space when the incoming constraints are
     * bounded, and collapses to zero (rather than reporting an infinite
     * size) when they aren't, e.g. inside a `Row`/`Column` with no
     * `Expanded`.
     */
    class Container : public StatelessWidget
    {
    public:
        WidgetRef                    child;
        std::optional<float>         width;
        std::optional<float>         height;
        std::optional<BoxConstraints> constraints;
        std::optional<Color>         color;
        std::optional<BoxDecoration> decoration;
        std::optional<BoxDecoration> foreground_decoration;
        std::optional<EdgeInsets>    padding;
        std::optional<EdgeInsets>    margin;
        std::optional<Alignment>     alignment;
        std::optional<Matrix4>       transform;
        std::optional<Alignment>     transform_alignment;
        Clip                         clip_behavior = Clip::none;

        Container() = default;
        explicit Container(WidgetRef c) : child(std::move(c)) {}

        /** @brief Full constructor for mw<>() convenience. */
        explicit Container(std::optional<float> w,
                           std::optional<float> h,
                           std::optional<Color> c,
                           std::optional<EdgeInsets> p,
                           std::optional<Alignment> a,
                           WidgetRef child_widget)
            : child(std::move(child_widget))
            , width(w)
            , height(h)
            , color(c)
            , padding(std::move(p))
            , alignment(std::move(a))
        {}

        WidgetRef build(BuildContext& context) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(!(color.has_value() && decoration.has_value()),
                "Container.color and Container.decoration cannot both be set "
                "-- set BoxDecoration.color instead");
            CW_ASSERT_MSG(clip_behavior == Clip::none || decoration.has_value() || color.has_value(),
                "Container.clip_behavior requires Container.color or Container.decoration to be set");
            if (width.has_value())
                CW_ASSERT_MSG(*width >= 0.0f, "Container.width must be non-negative");
            if (height.has_value())
                CW_ASSERT_MSG(*height >= 0.0f, "Container.height must be non-negative");
            if (constraints.has_value())
                CW_ASSERT_MSG(constraints->isNormalized(),
                    "Container.constraints must be normalized (0 <= min <= max)");
            if (padding.has_value()) {
                CW_ASSERT_MSG(padding->left >= 0.0f, "Container.padding.left must be non-negative");
                CW_ASSERT_MSG(padding->top >= 0.0f, "Container.padding.top must be non-negative");
                CW_ASSERT_MSG(padding->right >= 0.0f, "Container.padding.right must be non-negative");
                CW_ASSERT_MSG(padding->bottom >= 0.0f, "Container.padding.bottom must be non-negative");
            }
            if (margin.has_value()) {
                CW_ASSERT_MSG(margin->left >= 0.0f, "Container.margin.left must be non-negative");
                CW_ASSERT_MSG(margin->top >= 0.0f, "Container.margin.top must be non-negative");
                CW_ASSERT_MSG(margin->right >= 0.0f, "Container.margin.right must be non-negative");
                CW_ASSERT_MSG(margin->bottom >= 0.0f, "Container.margin.bottom must be non-negative");
            }
        }


    };

} // namespace systems::leal::campello_widgets
