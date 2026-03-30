#include <campello_widgets/ui/render_fractionally_sized_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderFractionallySizedBox::performLayout()
    {
        const float w = width_factor
            ? constraints_.max_width  * (*width_factor)
            : constraints_.max_width;
        const float h = height_factor
            ? constraints_.max_height * (*height_factor)
            : constraints_.max_height;

        size_ = constraints_.constrain({w, h});

        if (child_)
        {
            layoutChild(*child_, BoxConstraints::tight(size_));
            positionChild(*child_, alignment.inscribe(child_->size(), size_));
        }
    }

    void RenderFractionallySizedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
