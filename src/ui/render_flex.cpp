#include <algorithm>
#include <campello_widgets/ui/render_flex.hpp>

namespace systems::leal::campello_widgets
{

    void RenderFlex::insertChild(std::shared_ptr<RenderBox> box, int index, int flex)
    {
        if (index >= static_cast<int>(flex_children_.size()))
            flex_children_.resize(index + 1);
        if (flex_children_[index].box) flex_children_[index].box->setParent(nullptr);
        flex_children_[index] = {std::move(box), flex, {}};
        if (flex_children_[index].box) flex_children_[index].box->setParent(this);
        markNeedsLayout();
    }

    void RenderFlex::clearChildren()
    {
        for (auto& fc : flex_children_)
            if (fc.box) fc.box->setParent(nullptr);
        flex_children_.clear();
        markNeedsLayout();
    }

    void RenderFlex::performLayout()
    {
        const bool  is_h      = (axis == Axis::horizontal);
        const float max_main  = is_h ? constraints_.max_width  : constraints_.max_height;
        const float max_cross = is_h ? constraints_.max_height : constraints_.max_width;

        // First pass: lay out non-flex children.
        float allocated_main  = 0.0f;
        float max_cross_child = 0.0f;
        int   total_flex      = 0;

        for (auto& fc : flex_children_)
        {
            if (fc.flex > 0) { total_flex += fc.flex; continue; }

            BoxConstraints cc = is_h
                ? BoxConstraints{0.0f, std::max(0.0f, max_main - allocated_main), 0.0f, max_cross}
                : BoxConstraints{0.0f, max_cross, 0.0f, std::max(0.0f, max_main - allocated_main)};

            if (cross_axis_alignment == CrossAxisAlignment::stretch)
            {
                if (is_h) { cc.min_height = cc.max_height = max_cross; }
                else      { cc.min_width  = cc.max_width  = max_cross; }
            }

            fc.box->layout(cc);
            const Size s = fc.box->size();
            allocated_main  += is_h ? s.width : s.height;
            max_cross_child  = std::max(max_cross_child, is_h ? s.height : s.width);
        }

        // Second pass: lay out flex children.
        if (total_flex > 0)
        {
            const float free = std::max(0.0f, max_main - allocated_main);

            for (auto& fc : flex_children_)
            {
                if (fc.flex == 0) continue;

                const float main_size = (fc.flex / static_cast<float>(total_flex)) * free;

                BoxConstraints cc;
                if (cross_axis_alignment == CrossAxisAlignment::stretch)
                {
                    cc = is_h ? BoxConstraints::tight(main_size, max_cross)
                              : BoxConstraints::tight(max_cross, main_size);
                }
                else
                {
                    cc = is_h ? BoxConstraints{main_size, main_size, 0.0f, max_cross}
                              : BoxConstraints{0.0f, max_cross, main_size, main_size};
                }

                fc.box->layout(cc);
                const Size s = fc.box->size();
                allocated_main  += is_h ? s.width : s.height;
                max_cross_child  = std::max(max_cross_child, is_h ? s.height : s.width);
            }
        }

        // Compute own size.
        const float main_size  = (main_axis_size == MainAxisSize::max)
            ? max_main : allocated_main;
        const float cross_size = (cross_axis_alignment == CrossAxisAlignment::stretch)
            ? max_cross : max_cross_child;

        size_ = is_h
            ? constraints_.constrain({main_size, cross_size})
            : constraints_.constrain({cross_size, main_size});

        // Compute child positions.
        const float own_main = is_h ? size_.width : size_.height;
        const float free_main = own_main - allocated_main;

        float offset = 0.0f;
        float gap    = 0.0f;
        const auto n = static_cast<float>(flex_children_.size());

        switch (main_axis_alignment)
        {
            case MainAxisAlignment::start:
                offset = 0.0f; gap = 0.0f; break;
            case MainAxisAlignment::end:
                offset = free_main; gap = 0.0f; break;
            case MainAxisAlignment::center:
                offset = free_main * 0.5f; gap = 0.0f; break;
            case MainAxisAlignment::spaceBetween:
                offset = 0.0f;
                gap = (n > 1.0f) ? free_main / (n - 1.0f) : 0.0f;
                break;
            case MainAxisAlignment::spaceAround:
                gap    = (n > 0.0f) ? free_main / n : 0.0f;
                offset = gap * 0.5f;
                break;
            case MainAxisAlignment::spaceEvenly:
                gap    = (n > 0.0f) ? free_main / (n + 1.0f) : 0.0f;
                offset = gap;
                break;
        }

        const float cross_extent = is_h ? size_.height : size_.width;

        for (auto& fc : flex_children_)
        {
            const Size s          = fc.box->size();
            const float child_main  = is_h ? s.width  : s.height;
            const float child_cross = is_h ? s.height : s.width;

            float cross_offset = 0.0f;
            switch (cross_axis_alignment)
            {
                case CrossAxisAlignment::start:
                    cross_offset = 0.0f; break;
                case CrossAxisAlignment::end:
                    cross_offset = cross_extent - child_cross; break;
                case CrossAxisAlignment::center:
                    cross_offset = (cross_extent - child_cross) * 0.5f; break;
                case CrossAxisAlignment::stretch:
                    cross_offset = 0.0f; break;
            }

            fc.offset = is_h ? Offset{offset, cross_offset}
                              : Offset{cross_offset, offset};
            offset += child_main + gap;
        }
    }

    void RenderFlex::performPaint(PaintContext& context, const Offset& origin)
    {
        for (const auto& fc : flex_children_)
        {
            if (fc.box)
                fc.box->paint(context, {origin.x + fc.offset.x, origin.y + fc.offset.y});
        }
    }

    bool RenderFlex::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        // Walk back-to-front: last child is painted on top and gets first hit.
        for (auto it = flex_children_.rbegin(); it != flex_children_.rend(); ++it)
        {
            if (!it->box) continue;
            if (it->box->hitTest(result, position - it->offset))
                return true;
        }
        return false;
    }

} // namespace systems::leal::campello_widgets
