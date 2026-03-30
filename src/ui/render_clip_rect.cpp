#include <campello_widgets/ui/render_clip_rect.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    void RenderClipRect::performLayout()
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

    void RenderClipRect::performPaint(PaintContext& context, const Offset& offset)
    {
        Canvas& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));
        paintChild(context, offset);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
