#include <campello_widgets/ui/render_transform.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>

#include <cmath>

namespace systems::leal::campello_widgets
{

    // ----------------------------------------------------------------
    // Matrix helpers
    // ----------------------------------------------------------------

    RenderTransform::Matrix4 RenderTransform::rotation(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix4 m = Matrix4::identity();
        m.data[0] = c;   m.data[1] = s;
        m.data[4] = -s;  m.data[5] = c;
        return m;
    }

    RenderTransform::Matrix4 RenderTransform::scaling(float s)
    {
        return scaling(s, s);
    }

    RenderTransform::Matrix4 RenderTransform::scaling(float sx, float sy)
    {
        Matrix4 m = Matrix4::identity();
        m.data[0] = sx;
        m.data[5] = sy;
        return m;
    }

    RenderTransform::Matrix4 RenderTransform::translation(float dx, float dy)
    {
        return Matrix4::translate({dx, dy, 0.0f});
    }

    // ----------------------------------------------------------------
    // Layout
    // ----------------------------------------------------------------

    void RenderTransform::performLayout()
    {
        if (child_)
        {
            // Layout-transparent: pass constraints through unchanged, just like Flutter.
            const Size child_size = layoutChild(*child_, constraints_);
            size_         = child_size;
            child_offset_ = Offset::zero();
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    // ----------------------------------------------------------------
    // Paint
    // ----------------------------------------------------------------

    void RenderTransform::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!child_) return;

        Canvas& canvas = context.canvas();

        // Compute the pivot point in canvas (world) space.
        // The child's top-left in canvas space is (offset + child_offset_).
        // alignment maps to a point within the child's bounding box:
        //   x = child_tl.x + (1 + alignment.x) / 2 * child_width
        //   y = child_tl.y + (1 + alignment.y) / 2 * child_height
        const Offset child_tl = offset + child_offset_;
        const Size   child_sz = child_->size();
        const float  pivot_x  = child_tl.x + child_sz.width  * (1.0f + alignment.x) * 0.5f;
        const float  pivot_y  = child_tl.y + child_sz.height * (1.0f + alignment.y) * 0.5f;

        // Apply: translate-to-pivot, transform, translate-back.
        // This is equivalent to Flutter's pushTransform with an origin offset.
        canvas.save();
        canvas.translate(pivot_x, pivot_y);
        canvas.transform(transform);
        canvas.translate(-pivot_x, -pivot_y);
        paintChild(context, offset);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
