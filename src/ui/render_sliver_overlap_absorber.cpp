#include <campello_widgets/ui/render_sliver_overlap_absorber.hpp>

namespace systems::leal::campello_widgets
{

    void RenderSliverOverlapAbsorber::setChild(std::shared_ptr<RenderSliver> child)
    {
        if (child_ == child) return;
        if (child_) child_->setParent(nullptr);
        child_ = std::move(child);
        if (child_) child_->setParent(this);
        markNeedsLayout();
    }

    void RenderSliverOverlapAbsorber::performLayoutSliver()
    {
        if (!child_)
        {
            geometry_ = SliverGeometry{};
            if (handle) handle->layout_extent = 0.0f;
            return;
        }

        child_->layoutSliver(sliver_constraints_); // pure pass-through, unaltered
        geometry_ = child_->geometry();            // forward every field as-is
        if (handle) handle->layout_extent = geometry_.max_scroll_obstruction_extent;
    }

    void RenderSliverOverlapAbsorber::performPaint(PaintContext& context, const Offset& offset)
    {
        // RenderSliver::paint() is RenderObject::paint() -- works uniformly
        // for a RenderSliver child the same way it does for a RenderBox one.
        if (child_) child_->paint(context, offset);
    }

} // namespace systems::leal::campello_widgets
