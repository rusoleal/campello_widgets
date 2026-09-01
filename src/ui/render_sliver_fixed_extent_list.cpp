#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    void RenderSliverFixedExtentList::setItemBox(int index, std::shared_ptr<RenderBox> box)
    {
        auto it = item_boxes_.find(index);
        if (it != item_boxes_.end() && it->second.box == box) return;
        if (it != item_boxes_.end() && it->second.box) it->second.box->setParent(nullptr);
        if (box) box->setParent(this);
        item_boxes_[index] = ChildEntry{std::move(box), Offset{}};
        markNeedsLayout();
    }

    void RenderSliverFixedExtentList::removeItemBox(int index)
    {
        auto it = item_boxes_.find(index);
        if (it == item_boxes_.end()) return;
        if (it->second.box) it->second.box->setParent(nullptr);
        item_boxes_.erase(it);
        markNeedsLayout();
    }

    RenderBox* RenderSliverFixedExtentList::itemBoxAt(int index) const noexcept
    {
        auto it = item_boxes_.find(index);
        return (it != item_boxes_.end()) ? it->second.box.get() : nullptr;
    }

    int RenderSliverFixedExtentList::firstVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0) return 0;
        const float scroll = std::max(0.0f, sliver_constraints_.scroll_offset);
        return std::clamp(static_cast<int>(scroll / item_extent), 0, item_count - 1);
    }

    int RenderSliverFixedExtentList::lastVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0) return -1;
        const float scroll    = std::max(0.0f, sliver_constraints_.scroll_offset);
        const float remaining = sliver_constraints_.remaining_paint_extent;
        const int   last      = static_cast<int>((scroll + remaining) / item_extent);
        return std::clamp(last, 0, item_count - 1);
    }

    void RenderSliverFixedExtentList::performLayoutSliver()
    {
        const bool  is_v  = (sliver_constraints_.axis == Axis::vertical);
        const float cross = sliver_constraints_.cross_axis_extent;

        if (item_extent > 0.0f)
        {
            for (auto& [idx, entry] : item_boxes_)
            {
                if (!entry.box) continue;
                entry.box->layout(is_v ? BoxConstraints::tight(cross, item_extent)
                                        : BoxConstraints::tight(item_extent, cross));
                const float item_pos = static_cast<float>(idx) * item_extent;
                entry.offset = is_v ? Offset{0.0f, item_pos} : Offset{item_pos, 0.0f};
            }
        }

        const float content_extent = std::max(0.0f, static_cast<float>(item_count) * item_extent);
        const float scroll         = sliver_constraints_.scroll_offset;
        const float remaining      = sliver_constraints_.remaining_paint_extent;
        const float paint_extent   = std::clamp(
            std::min(content_extent, scroll + remaining) - scroll, 0.0f, remaining);

        geometry_.scroll_extent                 = content_extent;
        geometry_.paint_extent                  = paint_extent;
        geometry_.paint_origin                  = 0.0f;
        geometry_.layout_extent                 = paint_extent;
        geometry_.max_paint_extent              = content_extent;
        geometry_.max_scroll_obstruction_extent = 0.0f;
        geometry_.hit_test_extent               = paint_extent;
        geometry_.cache_extent                  = paint_extent;
        geometry_.scroll_offset_correction.reset();
        geometry_.visible             = paint_extent > 0.0f;
        geometry_.has_visual_overflow = (scroll > 0.0f) || (content_extent - scroll > remaining);

        checkVisibleRangeChanged();
    }

    void RenderSliverFixedExtentList::performPaint(PaintContext& context, const Offset& offset)
    {
        for (auto& [idx, entry] : item_boxes_)
        {
            if (entry.box) entry.box->paint(context, offset + entry.offset);
        }
    }

    void RenderSliverFixedExtentList::checkVisibleRangeChanged()
    {
        const int first = firstVisibleIndex();
        const int last  = lastVisibleIndex();

        if (first != cached_first_ || last != cached_last_)
        {
            cached_first_ = first;
            cached_last_  = last;
            if (on_visible_range_changed) on_visible_range_changed();
        }
    }

} // namespace systems::leal::campello_widgets
