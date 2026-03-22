#include <campello_widgets/ui/render_padding.hpp>

namespace systems::leal::campello_widgets
{

    void RenderPadding::performLayout()
    {
        if (child_)
        {
            const BoxConstraints child_constraints = constraints_.deflate(padding);
            const Size child_size = layoutChild(*child_, child_constraints);
            positionChild(*child_, {padding.left, padding.top});
            size_ = {child_size.width  + padding.horizontal(),
                     child_size.height + padding.vertical()};
        }
        else
        {
            size_ = constraints_.constrain(
                {padding.horizontal(), padding.vertical()});
        }
    }

    void RenderPadding::performPaint(PaintContext& context, const Offset& offset)
    {
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
