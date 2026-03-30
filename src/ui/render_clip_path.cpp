#include <campello_widgets/ui/render_clip_path.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    void RenderClipPath::performLayout()
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

    void RenderClipPath::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!clip_path_builder) { paintChild(context, offset); return; }

        Canvas& canvas = context.canvas();
        canvas.save();

        // Translate the path to the actual offset before clipping.
        Path clip = clip_path_builder(size_);
        clip.transform(Matrix4::translate({offset.x, offset.y, 0.0f}));
        canvas.clipPath(clip);

        paintChild(context, offset);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
