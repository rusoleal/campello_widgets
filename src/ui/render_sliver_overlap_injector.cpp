#include <campello_widgets/ui/render_sliver_overlap_injector.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    void RenderSliverOverlapInjector::performLayoutSliver()
    {
        const float gap_extent = handle ? handle->layout_extent : 0.0f;
        const float scroll     = sliver_constraints_.scroll_offset;
        const float remaining  = sliver_constraints_.remaining_paint_extent;

        // Same clamp as RenderSliverToBoxAdapter's own formula, gap_extent
        // standing in for a child's natural size.
        const float paint_extent = std::clamp(
            std::min(gap_extent, scroll + remaining) - scroll, 0.0f, remaining);

        geometry_.scroll_extent                 = gap_extent;
        geometry_.paint_extent                  = paint_extent;
        geometry_.paint_origin                  = 0.0f;
        geometry_.layout_extent                 = paint_extent;
        geometry_.max_paint_extent              = gap_extent;
        geometry_.max_scroll_obstruction_extent = 0.0f;
        geometry_.hit_test_extent               = 0.0f; // pure spacer -- nothing to hit
        geometry_.cache_extent                  = paint_extent;
        geometry_.scroll_offset_correction.reset();
        geometry_.visible             = paint_extent > 0.0f;
        geometry_.has_visual_overflow = (scroll > 0.0f) || (gap_extent - scroll > remaining);
    }

    void RenderSliverOverlapInjector::performPaint(PaintContext&, const Offset&)
    {
        // Nothing to paint -- pure spacer.
    }

} // namespace systems::leal::campello_widgets
