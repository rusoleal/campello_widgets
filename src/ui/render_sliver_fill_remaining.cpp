#include <campello_widgets/ui/render_sliver_fill_remaining.hpp>

namespace systems::leal::campello_widgets
{

    void RenderSliverFillRemaining::setChild(std::shared_ptr<RenderBox> child)
    {
        if (child_ == child) return;
        if (child_) child_->setParent(nullptr);
        child_ = std::move(child);
        if (child_) child_->setParent(this);
        markNeedsLayout();
    }

    void RenderSliverFillRemaining::performLayoutSliver()
    {
        const bool  is_v   = (sliver_constraints_.axis == Axis::vertical);
        const float cross  = sliver_constraints_.cross_axis_extent;
        const float extent = sliver_constraints_.remaining_paint_extent;

        if (child_)
        {
            child_->layout(is_v ? BoxConstraints{cross, cross, extent, extent}
                                 : BoxConstraints{extent, extent, cross, cross});
        }

        geometry_.scroll_extent                 = extent;
        geometry_.paint_extent                  = extent;
        geometry_.paint_origin                  = 0.0f;
        geometry_.layout_extent                 = extent;
        geometry_.max_paint_extent              = extent;
        geometry_.max_scroll_obstruction_extent = 0.0f;
        geometry_.hit_test_extent               = extent;
        geometry_.cache_extent                  = extent;
        geometry_.scroll_offset_correction.reset();
        geometry_.visible             = extent > 0.0f;
        geometry_.has_visual_overflow = false; // claims exactly its budget, by construction
    }

    void RenderSliverFillRemaining::performPaint(PaintContext& context, const Offset& offset)
    {
        if (child_) child_->paint(context, offset);
    }

} // namespace systems::leal::campello_widgets
