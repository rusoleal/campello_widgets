#include <campello_widgets/ui/render_sliver_persistent_header.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    void RenderSliverPersistentHeader::setChild(std::shared_ptr<RenderBox> child)
    {
        if (child_ == child) return;
        if (child_) child_->setParent(nullptr);
        child_ = std::move(child);
        if (child_) child_->setParent(this);
        markNeedsLayout();
    }

    void RenderSliverPersistentHeader::performLayoutSliver()
    {
        const bool  is_v    = (sliver_constraints_.axis == Axis::vertical);
        const float cross   = sliver_constraints_.cross_axis_extent;
        const float max_ext = std::max(min_extent, max_extent);
        const float scroll  = std::max(0.0f, sliver_constraints_.scroll_offset);
        const float shrink  = std::clamp(scroll, 0.0f, max_ext - min_extent);
        // max_ext -> min_extent as scroll grows, then holds at min_extent
        // forever after -- this holding-at-the-floor behavior, combined with
        // RenderViewport's own position clamp (gated on
        // max_scroll_obstruction_extent below), is what makes the header
        // pin instead of scrolling away.
        const float current_extent = max_ext - shrink;

        if (child_)
        {
            child_->layout(is_v ? BoxConstraints::tight(cross, current_extent)
                                 : BoxConstraints::tight(current_extent, cross));
        }

        const float remaining    = sliver_constraints_.remaining_paint_extent;
        const float paint_extent = std::clamp(current_extent, 0.0f, remaining);

        geometry_.scroll_extent                 = max_ext;
        geometry_.paint_extent                  = paint_extent;
        geometry_.paint_origin                  = 0.0f;
        geometry_.layout_extent                 = paint_extent;
        geometry_.max_paint_extent              = max_ext;
        // The resting/pinned floor, constant regardless of collapse phase --
        // this is what RenderViewport reads to decide this sliver is a
        // pinned obstruction and to compute pin_floor for later siblings.
        geometry_.max_scroll_obstruction_extent = min_extent;
        geometry_.hit_test_extent               = paint_extent;
        geometry_.cache_extent                  = paint_extent;
        geometry_.scroll_offset_correction.reset();
        geometry_.visible             = paint_extent > 0.0f;
        geometry_.has_visual_overflow = (shrink > 0.0f) || (paint_extent < current_extent);
    }

    void RenderSliverPersistentHeader::performPaint(PaintContext& context, const Offset& offset)
    {
        if (child_) child_->paint(context, offset);
    }

} // namespace systems::leal::campello_widgets
