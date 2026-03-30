#include <campello_widgets/ui/render_clip_oval.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/path.hpp>

namespace systems::leal::campello_widgets
{

    void RenderClipOval::performLayout()
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

    void RenderClipOval::performPaint(PaintContext& context, const Offset& offset)
    {
        Path clip;
        clip.addOval(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));

        Canvas& canvas = context.canvas();
        canvas.save();
        canvas.clipPath(clip);
        paintChild(context, offset);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
