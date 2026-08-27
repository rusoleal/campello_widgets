#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/offset_layer.hpp>

namespace systems::leal::campello_widgets
{

    /** @brief Controls whether the decoration is painted behind or in front of the child. */
    enum class DecorationPosition
    {
        background, ///< Decoration is painted below the child (default).
        foreground, ///< Decoration is painted above the child.
    };

    /**
     * @brief Whether painting `decoration` needs the offscreen-composite
     * path (box shadow blur, or a gradient fill/border via
     * `Canvas::beginShaderMask()`) rather than being a cheap direct draw.
     * See `RenderDecoratedBox`'s doc comment for why this drives its
     * `isRepaintBoundary()`/OffsetLayer-caching decision.
     */
    inline bool needsOffsetLayerFor(const BoxDecoration& decoration) noexcept
    {
        return !decoration.box_shadow.empty()
            || decoration.gradient.has_value()
            || (decoration.border.has_value() && decoration.border->gradient.has_value());
    }

    /**
     * @brief Paints a BoxDecoration around (or in front of) its child.
     *
     * Layout is pass-through: the child receives the full parent constraints and
     * this box adopts the child's size. With no child, the box fills the
     * available space.
     *
     * Paint order for DecorationPosition::background:
     *   1. Box shadows
     *   2. Background fill
     *   3. Border
     *   4. Child
     *
     * For DecorationPosition::foreground the child is painted first, then
     * steps 1–3.
     *
     * Self-boundaring (see `RenderClipRRect`'s doc comment for the general
     * mechanism/rationale), but only when `needsOffsetLayerFor(decoration)`
     * is true: `Renderer::applyBoxShadow()` (box shadows) and
     * `Canvas::beginShaderMask()` (gradient fill/border — resolved to a
     * `Shader` and composited via `applyShaderMask()`) both run their own
     * offscreen composite (a texture allocation plus two extra render-pass
     * restarts on the enclosing pass, and for gradients a from-scratch LUT
     * texture rebuild on top) — as expensive as `applyClipShape()` and with
     * the exact same problem, since plain `RenderObject::paint()` re-runs
     * `performPaint()` unconditionally on every visit from an animating
     * ancestor. Plain decorations (solid color/border only, no shadow, no
     * gradient) are cheap direct draws and skip the OffsetLayer machinery
     * entirely — `isRepaintBoundary()`/`paint()` both recompute
     * `needsOffsetLayerFor(decoration)` fresh each call, so a widget that
     * starts adding a shadow or gradient later picks up the caching
     * immediately.
     */
    class RenderDecoratedBox : public RenderBox
    {
    public:
        BoxDecoration     decoration;
        DecorationPosition position = DecorationPosition::background;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return needsOffsetLayerFor(decoration); }

    private:
        void paintDecoration(Canvas& canvas, const Offset& offset) const;

        OffsetLayer offset_layer_;
    };

} // namespace systems::leal::campello_widgets
