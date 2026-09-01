#pragma once

#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/sliver_constraints.hpp>
#include <campello_widgets/ui/sliver_geometry.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Base class for render objects that use the sliver layout
     * protocol — a second direct subclass of RenderObject, sitting
     * alongside RenderBox, not through it.
     *
     * RenderObject::layout()/performLayout()/size()/constraints() are
     * hardcoded to BoxConstraints/Size (see RenderObject's own doc and
     * `constraints_`/`size_`'s declarations) — a RenderSliver cannot use
     * them for its own layout. Instead it gets layoutSliver()/geometry(),
     * a parallel entry point taking SliverConstraints and producing
     * SliverGeometry, following the exact same dirty-flag structure as
     * RenderObject::layout() (see layoutSliver()'s implementation).
     *
     * Key design decision: rather than leaving the inherited
     * `constraints_`/`size_` dead and unused, layoutSliver() deliberately
     * *repurposes* the inherited `size_` as a synthesized 2D bounding box
     * derived from geometry_ (paint_extent along the scroll axis,
     * cross_axis_extent along the other) every time it lays out. This
     * lets RenderSliver reuse the *entire* inherited paint() unchanged —
     * its dirty-region bookkeeping, repaint-rainbow overlay, and
     * paintSizeEnabled debug border all already read `size_` internally
     * (see RenderObject::paint()'s body), and duplicating that logic in a
     * RenderSliver-specific paint() override would mean re-guessing
     * behavior that's already proven correct for every RenderBox. This is
     * deliberate, narrow reuse of one field for one purpose (dirty-region
     * size) — it does not mean "slivers are secretly boxes"; every other
     * box-shaped concept (BoxConstraints, hit-testing) has no bearing on
     * RenderSliver at all.
     *
     * Hit-testing is intentionally absent at this stage — it's declared
     * entirely on RenderBox, not RenderObject, with no generic precedent
     * to inherit, and nothing yet produces real sliver content to hit.
     * See the Sliver Protocol Scoping artifact for the later stage this
     * belongs to.
     */
    class RenderSliver : public RenderObject
    {
    public:
        /**
         * @brief Runs the sliver layout pass for this render object —
         * the sliver-protocol analog of RenderObject::layout().
         *
         * Stores the incoming constraints, delegates to
         * performLayoutSliver() if dirty or the constraints changed, then
         * clears the layout-dirty flag and synthesizes `size_` (see this
         * class's own doc comment) from the resulting geometry.
         */
        void layoutSliver(const SliverConstraints& constraints);

        /** @brief The geometry computed by the most recent sliver layout pass. */
        const SliverGeometry& geometry() const noexcept { return geometry_; }

        /** @brief The constraints from the most recent sliver layout pass. */
        const SliverConstraints& sliverConstraints() const noexcept { return sliver_constraints_; }

    protected:
        /**
         * @brief Computes geometry_ based on sliver_constraints_ (already
         * set by layoutSliver() before this runs) and any children.
         *
         * Override in subclasses. Must set geometry_ before returning —
         * the sliver-protocol analog of RenderBox::performLayout()
         * setting size_.
         */
        virtual void performLayoutSliver() = 0;

        // Written directly by performLayoutSliver() overrides, the same
        // way RenderBox subclasses write the inherited size_ directly
        // from performLayout() -- protected, not accessed through a
        // setter, to match that existing convention.
        SliverConstraints sliver_constraints_{};
        SliverGeometry     geometry_{};

    private:
        // RenderObject::performLayout() is pure virtual -- every subclass
        // must implement it or stay abstract. RenderSliver instances are
        // laid out exclusively via layoutSliver(), never the inherited,
        // box-typed layout(BoxConstraints) -- this override exists purely
        // to satisfy that requirement and guard against the box-typed path
        // being reached by mistake, rather than to do real work. `final`
        // additionally stops any further subclass from silently
        // reimplementing it instead of performLayoutSliver().
        void performLayout() final;
    };

} // namespace systems::leal::campello_widgets
