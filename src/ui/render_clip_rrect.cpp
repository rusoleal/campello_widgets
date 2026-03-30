#include <campello_widgets/ui/render_clip_rrect.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    void RenderClipRRect::performLayout()
    {
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    void RenderClipRRect::performPaint(PaintContext& context, const Offset& offset)
    {
        Canvas& canvas = context.canvas();
        canvas.save();
        canvas.clipRRect(RRect{
            Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
            border_radius
        });
        paintChild(context, offset);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
