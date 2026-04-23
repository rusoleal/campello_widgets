#pragma once

#include <memory>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/hit_test.hpp>

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
         * Default: true whenever position falls within [0, size). Override to
         * make a box transparent to pointer events.
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

        std::vector<std::shared_ptr<DiagnosticsNode>> debugDescribeChildren() const override;

        /**
         * @brief Visits all render children for diagnostic tree traversal.
         *
         * Override in multi-child render boxes (RenderFlex, RenderStack, etc.)
         * to enumerate all children. Default implementation visits only `child_`.
         */
        virtual void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const;

    protected:

    protected:
        std::shared_ptr<RenderBox> child_;
        Offset                     child_offset_;
    };

} // namespace systems::leal::campello_widgets
