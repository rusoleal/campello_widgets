#pragma once

#include <memory>
#include <optional>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/text_baseline.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/dirty_region.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderObject that uses the box layout model.
     *
     * RenderBox adds:
     *  - A single optional child render box.
     *  - `layoutChild()` / `positionChild()` helpers for managing that child.
     *
     * Most concrete render objects (Container, Padding, SizedBox, etc.) inherit
     * from RenderBox. Multi-child render objects (Row, Column, Stack) will also
     * extend RenderBox and manage a list of children directly.
     */
    class RenderBox : public RenderObject
    {
    public:
        // ------------------------------------------------------------------
        // Child management
        // ------------------------------------------------------------------

        void setChild(std::shared_ptr<RenderBox> child) noexcept;

        RenderBox* child() const noexcept { return child_.get(); }

        // ------------------------------------------------------------------
        // Layout helpers
        // ------------------------------------------------------------------

        /**
         * @brief Lays out the given child with the provided constraints.
         *
         * Delegates to `child.layout()` and returns the child's resulting size.
         *
         * @param child       The child render box to lay out.
         * @param constraints Constraints to pass down to the child.
         * @return The size the child chose for itself.
         */
        Size layoutChild(RenderBox& child, const BoxConstraints& constraints);

        /**
         * @brief Records the offset at which `child` will be painted.
         *
         * Does not trigger a repaint — the stored offset is used during the
         * next paint pass when `paintChild()` is called.
         *
         * @param child  The child whose position to set.
         * @param offset Top-left corner of the child in this box's coordinate space.
         */
        void positionChild(RenderBox& child, const Offset& offset) noexcept;

        /**
         * @brief Paints the child at its stored offset.
         *
         * Call this from `performPaint()` after painting the background (if any).
         *
         * @param context The current paint context.
         * @param origin  This box's own origin (passed in from performPaint).
         */
        void paintChild(PaintContext& context, const Offset& origin) const;

        // ------------------------------------------------------------------
        // Default implementations
        // ------------------------------------------------------------------

        /**
         * @brief Default layout: becomes as large as constraints allow, then
         * lays out and centres the child if one is present.
         */
        void performLayout() override;

        /**
         * @brief Default paint: paints the child (if any) at its stored offset.
         */
        void performPaint(PaintContext& context, const Offset& offset) override;

        // ------------------------------------------------------------------
        // Hit testing
        // ------------------------------------------------------------------

        /**
         * @brief Entry point for hit-testing at `position` (local coordinates).
         *
         * Returns true if this box or any descendant was hit. On a hit, a
         * HitTestEntry for this box is appended to `result` (deepest first).
         *
         * The default implementation: bounds-checks, recurses into children
         * via `hitTestChildren()`, then calls `hitTestSelf()`.
         *
         * @param result   Accumulator; entries are added deepest first.
         * @param position Hit point in this box's local coordinate space.
         */
        virtual bool hitTest(HitTestResult& result, const Offset& position);

        /**
         * @brief Returns true if `position` (local coords) hits this box itself.
         *
         * Default: false — a plain layout box does not claim pointer events
         * on its own account. Override to true in any RenderBox that
         * registers its own pointer handling (GestureDetector, TextField,
         * Slider, Draggable, scrollables, MouseRegion, ...), so it still
         * receives hits at points not claimed by a more specific descendant.
         */
        virtual bool hitTestSelf(const Offset& position) const;

        /**
         * @brief Recurses into child boxes and returns true if any was hit.
         *
         * Default: delegates to the single `child_` using `child_offset_`.
         * RenderFlex and RenderStack override this to walk their child lists.
         *
         * @param result   Accumulator.
         * @param position Hit point in this box's local coordinate space.
         */
        virtual bool hitTestChildren(HitTestResult& result, const Offset& position);

        // ------------------------------------------------------------------
        // Diagnostics
        // ------------------------------------------------------------------

        // ------------------------------------------------------------------
        // Baseline
        // ------------------------------------------------------------------

        /**
         * @brief Distance from this box's top edge down to its baseline, if
         * it has one.
         *
         * Default: delegates to `child_` at `child_offset_` if present
         * (adding the child's own offset), else returns `std::nullopt` --
         * mirroring Flutter's `RenderBox.computeDistanceToActualBaseline()`
         * default. Text render objects (`RenderText`, `RenderParagraph`)
         * override this with a real measurement; `RenderBaseline` is the
         * only other caller.
         */
        virtual std::optional<float> computeDistanceToActualBaseline(TextBaseline baseline) const;

        std::vector<std::shared_ptr<DiagnosticsNode>> debugDescribeChildren() const override;

        /**
         * @brief Visits all render children for diagnostic tree traversal.
         *
         * Override in multi-child render boxes (RenderFlex, RenderStack, etc.)
         * to enumerate all children. Default implementation visits only `child_`.
         */
        virtual void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const;

    protected:

        /**
         * @brief Computes this box's true on-screen rect as of the current paint
         * pass -- accounting for the safe-area paint-origin inset
         * (RenderObject::activePaintOriginOffset()) and any ambient canvas
         * transform (e.g. a scrolled ancestor's canvas.translate(), independent
         * of `offset` and only applied at paint time). Call from performPaint()
         * with the same context/offset it received, and store the result into
         * your own field if needed outside the paint pass -- this is not cached.
         *
         * The shared recipe RenderFocus/RenderGestureDetector each independently
         * implemented before this existed. Note RenderDraggable's own
         * global-offset tracking deliberately does NOT use this -- it only
         * subtracts the paint-origin inset, skipping the ambient-transform
         * projection below, a real pre-existing divergence left as-is here.
         */
        Rect computeGlobalRect(PaintContext& context, const Offset& offset) const noexcept
        {
            const Offset paint_origin = RenderObject::activePaintOriginOffset();
            const Rect   local_bounds = Rect::fromLTWH(
                offset.x - paint_origin.x, offset.y - paint_origin.y,
                size_.width, size_.height);
            return projectedBounds(context.canvas().currentTransform(), local_bounds);
        }

    protected:
        std::shared_ptr<RenderBox> child_;
        Offset                     child_offset_;
    };

} // namespace systems::leal::campello_widgets
