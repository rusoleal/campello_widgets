#include <campello_widgets/ui/render_colored_box.hpp>
#include <cmath>
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
            // Fill the bounded max on each axis; an unbounded axis falls
            // back to 0 (clamped up to min_width/min_height by constrain()
            // below) instead of reporting an infinite size -- mirrors
            // Flutter's Container/ColoredBox behavior inside e.g. a Row/
            // Column with no Expanded, where the incoming max is infinite.
            const Size fill{
                std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width,
                std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height,
            };
            size_ = constraints_.constrain(fill);
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
