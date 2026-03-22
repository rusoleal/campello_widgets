#include <algorithm>
#include <campello_widgets/ui/render_sized_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderSizedBox::performLayout()
    {
        const float w = width  ? std::clamp(*width,  constraints_.min_width,  constraints_.max_width)
                                : constraints_.max_width;
        const float h = height ? std::clamp(*height, constraints_.min_height, constraints_.max_height)
                                : constraints_.max_height;
        size_ = {w, h};

        if (child_)
        {
            layoutChild(*child_, BoxConstraints::tight(size_));
            positionChild(*child_, {0.0f, 0.0f});
        }
    }

    void RenderSizedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
