#include <campello_widgets/ui/render_repaint_boundary.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    void RenderRepaintBoundary::performLayout()
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

    void RenderRepaintBoundary::paint(PaintContext& context, const Offset& offset)
    {
        // Recording into the *live* context (rather than a separate local/
        // headless one) matters: PushClipRectCmd bakes an absolute rect at
        // record time (unlike draw-command geometry, which is
        // transform-deferred to flush time — see flushDrawList()'s
        // `current_clip = c.rect;`), so a clip recorded against a
        // fabricated local origin would replay at the wrong position once
        // translated. Recording at the real `offset` keeps every command —
        // including clips — correct for *this* offset. See OffsetLayer's
        // doc for the mechanism (identity replay, cheap delta-translate
        // reposition when safe, full re-record fallback otherwise).
        if (!offset_layer_.maybeReplay(context, offset, size_, needsPaint()))
            offset_layer_.record(context, offset, [&] { paintChild(context, offset); });

        needs_paint_ = false;
    }

} // namespace systems::leal::campello_widgets
