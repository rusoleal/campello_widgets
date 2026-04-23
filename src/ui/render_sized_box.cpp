#include <algorithm>
#include <campello_widgets/ui/render_sized_box.hpp>

namespace systems::leal::campello_widgets
{

    void RenderSizedBox::performLayout()
    {
        const BoxConstraints additional = BoxConstraints::tightFor(width, height);

        if (child_)
        {
            layoutChild(*child_, additional.enforce(constraints_));
            size_ = constraints_.constrain(child_->size());
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = additional.enforce(constraints_).constrain(Size::zero());
        }
    }

    void RenderSizedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
