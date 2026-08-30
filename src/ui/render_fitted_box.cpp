#include <campello_widgets/ui/render_fitted_box.hpp>
#include <campello_widgets/ui/render_transform.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    void RenderFittedBox::performLayout()
    {
        if (child_)
        {
            // Unconstrained-ish layout so the child reports its natural
            // preferred size -- mirrors RenderAlign's identical reasoning.
            const Size child_size = layoutChild(*child_, constraints_.loosen());
            (void)child_size;
            // Painted through a canvas transform, not positioned via the
            // normal child_offset_ mechanism -- see performPaint().
            child_offset_ = Offset::zero();
        }

        // Fills the space the parent gives it, like RenderAlign with no
        // size factors -- the child is scaled to fit *this* size, not the
        // other way around.
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});
    }

    void RenderFittedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!child_) return;

        const float bw = size_.width;
        const float bh = size_.height;
        const float tw = child_->size().width;
        const float th = child_->size().height;

        if (bw <= 0.0f || bh <= 0.0f || tw <= 0.0f || th <= 0.0f)
            return;

        float sx = 1.0f, sy = 1.0f;
        switch (fit)
        {
            case BoxFit::fill:
                sx = bw / tw;
                sy = bh / th;
                break;
            case BoxFit::contain:
                sx = sy = std::min(bw / tw, bh / th);
                break;
            case BoxFit::cover:
                sx = sy = std::max(bw / tw, bh / th);
                break;
            case BoxFit::fitWidth:
                sx = sy = bw / tw;
                break;
            case BoxFit::fitHeight:
                sx = sy = bh / th;
                break;
            case BoxFit::none:
                sx = sy = 1.0f;
                break;
            case BoxFit::scaleDown:
                sx = sy = std::min(1.0f, std::min(bw / tw, bh / th));
                break;
        }

        const Offset dst_pos = alignment.inscribe({tw * sx, th * sy}, {bw, bh});

        Canvas& canvas = context.canvas();
        canvas.save();
        canvas.translate(offset.x + dst_pos.x, offset.y + dst_pos.y);
        canvas.transform(RenderTransform::scaling(sx, sy));
        paintChild(context, Offset::zero());
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
