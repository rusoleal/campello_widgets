#include <campello_widgets/ui/render_opacity.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>

namespace systems::leal::campello_widgets
{

    RenderOpacity::RenderOpacity(float opacity)
        : opacity_(opacity)
    {
    }

    void RenderOpacity::setOpacity(float opacity) noexcept
    {
        if (opacity_ == opacity) return;
        opacity_ = opacity;
        markNeedsPaint();
    }

    void RenderOpacity::performLayout()
    {
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
        }
        else
        {
            size_ = Size{constraints_.min_width, constraints_.min_height};
        }
    }

    void RenderOpacity::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!child_) return;

        context.canvas().save();
        context.canvas().setOpacity(opacity_);
        paintChild(context, offset);
        context.canvas().restore();
    }

} // namespace systems::leal::campello_widgets
