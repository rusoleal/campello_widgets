#include <algorithm>
#include <vector>
#include <campello_widgets/ui/render_wrap.hpp>

namespace systems::leal::campello_widgets
{

    // -----------------------------------------------------------------------
    // Child management
    // -----------------------------------------------------------------------

    void RenderWrap::insertChild(std::shared_ptr<RenderBox> box, int index)
    {
        if (index >= static_cast<int>(wrap_children_.size()))
            wrap_children_.resize(index + 1);
        if (wrap_children_[index].box)
            wrap_children_[index].box->setParent(nullptr);
        wrap_children_[index] = {std::move(box), {}};
        if (wrap_children_[index].box)
            wrap_children_[index].box->setParent(this);
        markNeedsLayout();
    }

    void RenderWrap::clearChildren()
    {
        for (auto& wc : wrap_children_)
            if (wc.box) wc.box->setParent(nullptr);
        wrap_children_.clear();
        markNeedsLayout();
    }

    // -----------------------------------------------------------------------
    // Helper: distribute `extra` space across `count` slots per alignment
    // -----------------------------------------------------------------------

    static void distributeSpace(WrapAlignment align, float extra, int count,
                                float& leading, float& between)
    {
        if (count <= 0) { leading = 0; between = 0; return; }
        switch (align)
        {
        case WrapAlignment::start:
            leading = 0; between = 0; break;
        case WrapAlignment::end:
            leading = extra; between = 0; break;
        case WrapAlignment::center:
            leading = extra * 0.5f; between = 0; break;
        case WrapAlignment::space_between:
            leading = 0;
            between = (count > 1) ? extra / float(count - 1) : 0;
            break;
        case WrapAlignment::space_around:
            between = extra / float(count);
            leading = between * 0.5f;
            break;
        case WrapAlignment::space_evenly:
            between = extra / float(count + 1);
            leading = between;
            break;
        }
    }

    static void distributeRunSpace(WrapRunAlignment align, float extra, int count,
                                   float& leading, float& between)
    {
        if (count <= 0) { leading = 0; between = 0; return; }
        switch (align)
        {
        case WrapRunAlignment::start:
            leading = 0; between = 0; break;
        case WrapRunAlignment::end:
            leading = extra; between = 0; break;
        case WrapRunAlignment::center:
            leading = extra * 0.5f; between = 0; break;
        case WrapRunAlignment::space_between:
            leading = 0;
            between = (count > 1) ? extra / float(count - 1) : 0;
            break;
        case WrapRunAlignment::space_around:
            between = extra / float(count);
            leading = between * 0.5f;
            break;
        case WrapRunAlignment::space_evenly:
            between = extra / float(count + 1);
            leading = between;
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------

    void RenderWrap::performLayout()
    {
        const bool  horizontal = (direction == Axis::horizontal);
        const float main_max   = horizontal ? constraints_.max_width  : constraints_.max_height;
        const float cross_max  = horizontal ? constraints_.max_height : constraints_.max_width;

        // ── Pass 1: measure every child ────────────────────────────────────
        struct ChildInfo
        {
            RenderBox* box;
            float      main;
            float      cross;
        };
        std::vector<ChildInfo> infos;
        infos.reserve(wrap_children_.size());

        const BoxConstraints measure = horizontal
            ? BoxConstraints{0, main_max, 0, cross_max}
            : BoxConstraints{0, cross_max, 0, main_max};

        for (auto& wc : wrap_children_)
        {
            if (!wc.box) continue;
            wc.box->layout(measure);
            const Size s = wc.box->size();
            infos.push_back({wc.box.get(),
                             horizontal ? s.width  : s.height,
                             horizontal ? s.height : s.width});
        }

        // ── Pass 2: group into runs ────────────────────────────────────────
        struct Run { int start; int count; float main_size; float cross_size; };
        std::vector<Run> runs;
        {
            const int n = int(infos.size());
            int i = 0;
            while (i < n)
            {
                float run_main  = 0.0f;
                float run_cross = 0.0f;
                int   start     = i;
                int   count     = 0;
                while (i < n)
                {
                    float needed = run_main + infos[i].main + (count > 0 ? spacing : 0.0f);
                    if (count > 0 && needed > main_max + 0.5f) break;
                    run_main  = needed;
                    run_cross = std::max(run_cross, infos[i].cross);
                    ++count; ++i;
                }
                runs.push_back({start, count, run_main, run_cross});
            }
        }

        // ── Pass 3: compute own size ───────────────────────────────────────
        float total_cross = 0.0f;
        for (int r = 0; r < int(runs.size()); ++r)
        {
            total_cross += runs[r].cross_size;
            if (r + 1 < int(runs.size())) total_cross += run_spacing;
        }

        const float own_main  = main_max;
        const float own_cross = std::clamp(total_cross,
            horizontal ? constraints_.min_height : constraints_.min_width, cross_max);

        size_ = horizontal ? Size{own_main, own_cross} : Size{own_cross, own_main};

        // ── Pass 4: position every child ──────────────────────────────────
        float extra_cross = own_cross - total_cross;
        float run_leading, run_between;
        distributeRunSpace(run_alignment, extra_cross, int(runs.size()),
                           run_leading, run_between);

        float cross_offset = run_leading;
        int   info_idx     = 0;  // running index into infos (parallel to wrap_children_)

        for (auto& run : runs)
        {
            const float total_spacing = spacing * float(run.count - 1);
            const float extra_main    = own_main - run.main_size - total_spacing;
            float child_leading, child_between;
            distributeSpace(alignment, extra_main, run.count,
                            child_leading, child_between);

            float main_offset = child_leading;
            for (int j = run.start; j < run.start + run.count; ++j)
            {
                const ChildInfo& ci = infos[j];

                float cross_child = cross_offset;
                switch (cross_axis_alignment)
                {
                case WrapCrossAlignment::start:   break;
                case WrapCrossAlignment::end:     cross_child += run.cross_size - ci.cross; break;
                case WrapCrossAlignment::center:  cross_child += (run.cross_size - ci.cross) * 0.5f; break;
                }

                // Find the matching WrapChild and store the offset.
                // infos[j] was built in child order, so wrap_children_[j].box == ci.box.
                wrap_children_[j].offset = horizontal
                    ? Offset{main_offset, cross_child}
                    : Offset{cross_child, main_offset};

                main_offset += ci.main + spacing + child_between;
            }

            cross_offset += run.cross_size + run_spacing + run_between;
            info_idx     += run.count;
        }

        (void)info_idx;
    }

    // -----------------------------------------------------------------------
    // Paint
    // -----------------------------------------------------------------------

    void RenderWrap::performPaint(PaintContext& context, const Offset& offset)
    {
        for (const auto& wc : wrap_children_)
        {
            if (wc.box)
                wc.box->paint(context, {offset.x + wc.offset.x, offset.y + wc.offset.y});
        }
    }

    // -----------------------------------------------------------------------
    // Hit testing
    // -----------------------------------------------------------------------

    bool RenderWrap::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        for (auto it = wrap_children_.rbegin(); it != wrap_children_.rend(); ++it)
        {
            if (!it->box) continue;
            if (it->box->hitTest(result, position - it->offset))
                return true;
        }
        return false;
    }

} // namespace systems::leal::campello_widgets
