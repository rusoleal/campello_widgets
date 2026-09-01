#include <campello_widgets/ui/render_sliver_to_box_adapter.hpp>
#include <algorithm>
#include <limits>

namespace systems::leal::campello_widgets
{

    void RenderSliverToBoxAdapter::setChild(std::shared_ptr<RenderBox> child)
    {
        if (child_ == child) return;
        if (child_) child_->setParent(nullptr);
        child_ = std::move(child);
        if (child_) child_->setParent(this);
        markNeedsLayout();
    }

    void RenderSliverToBoxAdapter::performLayoutSliver()
    {
        if (!child_) { geometry_ = SliverGeometry{}; return; }

        const bool  is_v  = (sliver_constraints_.axis == Axis::vertical);
        const float cross = sliver_constraints_.cross_axis_extent;
        const float inf   = std::numeric_limits<float>::infinity();

        child_->layout(is_v ? BoxConstraints{cross, cross, 0.0f, inf}
                             : BoxConstraints{0.0f, inf, cross, cross});

        const float child_extent = is_v ? child_->size().height : child_->size().width;
        const float scroll    = sliver_constraints_.scroll_offset;
        const float remaining = sliver_constraints_.remaining_paint_extent;

        // The portion of [0, child_extent) that falls inside [scroll, scroll+remaining).
        const float paint_extent = std::clamp(
            std::min(child_extent, scroll + remaining) - scroll, 0.0f, remaining);

        geometry_.scroll_extent                 = child_extent;
        geometry_.paint_extent                  = paint_extent;
        geometry_.paint_origin                  = 0.0f;   // no persistent-header behavior until Stage 5
        geometry_.layout_extent                 = paint_extent;
        geometry_.max_paint_extent              = child_extent;
        geometry_.max_scroll_obstruction_extent = 0.0f;
        geometry_.hit_test_extent               = paint_extent;
        geometry_.cache_extent                  = paint_extent;  // no separate cache region yet -- matches RenderViewport's own Stage 2 deferral (remaining_cache_extent == remaining_paint_extent today)
        geometry_.scroll_offset_correction.reset();
        geometry_.visible             = paint_extent > 0.0f;
        geometry_.has_visual_overflow = (scroll > 0.0f) || (child_extent - scroll > remaining);
    }

    void RenderSliverToBoxAdapter::performPaint(PaintContext& context, const Offset& offset)
    {
        if (child_) child_->paint(context, offset);
    }

} // namespace systems::leal::campello_widgets
