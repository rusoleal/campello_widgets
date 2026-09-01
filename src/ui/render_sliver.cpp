#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    void RenderSliver::layoutSliver(const SliverConstraints& constraints)
    {
        // Mirrors RenderObject::layout()'s exact dirty-flag structure,
        // reusing the inherited protected needs_layout_/markNeedsPaint()
        // directly rather than duplicating flag state.
        const bool constraints_changed = !(constraints == sliver_constraints_);

        if (!needs_layout_ && !constraints_changed)
            return;

        const bool must_repaint = constraints_changed;
        sliver_constraints_ = constraints;
        needs_layout_        = false;
        performLayoutSliver();

        // Repurposed dirty-region bounding box -- see class doc comment.
        size_ = (constraints.axis == Axis::horizontal)
            ? Size{geometry_.paint_extent, constraints.cross_axis_extent}
            : Size{constraints.cross_axis_extent, geometry_.paint_extent};

        if (must_repaint)
            markNeedsPaint();
    }

    void RenderSliver::performLayout()
    {
        // Reached only if something calls the inherited, box-typed
        // layout(BoxConstraints) on a RenderSliver directly, which is
        // always a caller bug -- RenderSliver instances are laid out
        // exclusively via layoutSliver(). Fails loudly rather than
        // silently leaving size_/constraints_ at their garbage defaults.
        CW_ASSERT_MSG(false,
            "RenderSliver::performLayout() reached -- call layoutSliver(const SliverConstraints&) instead of layout(const BoxConstraints&) on a RenderSliver.");
    }

} // namespace systems::leal::campello_widgets
