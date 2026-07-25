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

    void RenderClipRRect::paint(PaintContext& context, const Offset& offset)
    {
        // See this class's doc comment and RenderRepaintBoundary::paint()
        // for the mechanism: replay the cached composite when this node is
        // clean and its offset hasn't moved, otherwise record fresh. OR in
        // needsDescendantPaint() too — see its doc comment for why a
        // replay must not skip past a nested boundary with unconsumed
        // dirty state.
        if (!offset_layer_.maybeReplay(context, offset, size_,
                                        needsPaint(), needsDescendantPaint()))
            offset_layer_.record(context, offset, [&] { performPaint(context, offset); });

        needs_paint_ = false;
        needs_descendant_paint_ = false;
    }

} // namespace systems::leal::campello_widgets
