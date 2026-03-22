#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    void RenderColoredBox::performLayout()
    {
        if (child_)
        {
            const Size child_size = layoutChild(*child_, constraints_.loosen());
            size_ = constraints_.constrain(child_size);
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});
        }
    }

    void RenderColoredBox::performPaint(PaintContext& context, const Offset& offset)
    {
        context.canvas().drawRect(
            Rect::fromOffsetAndSize(offset, size_),
            Paint::filled(color));
        paintChild(context, offset);
    }

} // namespace systems::leal::campello_widgets
