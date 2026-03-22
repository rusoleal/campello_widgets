#include <campello_widgets/ui/render_align.hpp>

namespace systems::leal::campello_widgets
{

    void RenderAlign::performLayout()
    {
        if (child_)
        {
            const Size child_size = layoutChild(*child_, constraints_.loosen());

            const float w = width_factor
                ? child_size.width  * (*width_factor)
                : constraints_.max_width;
            const float h = height_factor
                ? child_size.height * (*height_factor)
                : constraints_.max_height;

            size_ = constraints_.constrain({w, h});
            positionChild(*child_, alignment.inscribe(child_size, size_));
        }
        else
        {
            size_ = constraints_.constrain(
                {constraints_.max_width, constraints_.max_height});
        }
    }

    void RenderAlign::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
