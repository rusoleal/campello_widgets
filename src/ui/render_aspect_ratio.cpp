#include <algorithm>
#include <campello_widgets/ui/render_aspect_ratio.hpp>

namespace systems::leal::campello_widgets
{

    void RenderAspectRatio::performLayout()
    {
        const float ar = (aspect_ratio > 0.0f) ? aspect_ratio : 1.0f;

        // Try fitting by width first.
        float w = constraints_.max_width;
        float h = w / ar;

        if (h > constraints_.max_height)
        {
            // Doesn't fit; try fitting by height.
            h = constraints_.max_height;
            w = h * ar;
        }

        // Clamp to constraints.
        w = std::clamp(w, constraints_.min_width,  constraints_.max_width);
        h = std::clamp(h, constraints_.min_height, constraints_.max_height);
        size_ = {w, h};

        if (child_)
        {
            layoutChild(*child_, BoxConstraints::tight(size_));
            positionChild(*child_, {0.0f, 0.0f});
        }
    }

    void RenderAspectRatio::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
