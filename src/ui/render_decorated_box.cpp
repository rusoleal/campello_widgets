#include <campello_widgets/ui/render_decorated_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/paint.hpp>

namespace systems::leal::campello_widgets
{

    void RenderDecoratedBox::performLayout()
    {
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
            positionChild(*child_, {0.0f, 0.0f});
        }
        else
        {
            size_ = constraints_.constrain(
                {constraints_.max_width, constraints_.max_height});
        }
    }

    void RenderDecoratedBox::performPaint(PaintContext& context, const Offset& offset)
    {
        if (position == DecorationPosition::background)
        {
            paintDecoration(context.canvas(), offset);
            paintChild(context, offset);
        }
        else
        {
            paintChild(context, offset);
            paintDecoration(context.canvas(), offset);
        }
    }

    void RenderDecoratedBox::paintDecoration(Canvas& canvas, const Offset& offset) const
    {
        const bool  has_radius = decoration.border_radius > 0.0f;
        const Rect  bounds     = Rect::fromLTWH(offset.x, offset.y,
                                                size_.width, size_.height);
        const RRect rbounds    = RRect{bounds, decoration.border_radius};

        // 1. Box shadows
        for (const BoxShadow& shadow : decoration.box_shadow)
        {
            // Build a path for the shadow shape, displaced by shadow.offset.
            const Rect shadow_bounds = Rect::fromLTWH(
                bounds.x + shadow.offset.x - shadow.spread_radius,
                bounds.y + shadow.offset.y - shadow.spread_radius,
                bounds.width  + 2.0f * shadow.spread_radius,
                bounds.height + 2.0f * shadow.spread_radius);

            Path shadow_path;
            if (has_radius)
                shadow_path.addRRect(RRect{shadow_bounds, decoration.border_radius});
            else
                shadow_path.addRect(shadow_bounds);

            // blur_radius is used as the elevation approximation.
            canvas.drawShadow(shadow_path, shadow.color, shadow.blur_radius, false);
        }

        // 2. Background fill
        if (decoration.color.has_value())
        {
            const Paint fill = Paint::filled(*decoration.color);
            if (has_radius)
                canvas.drawRRect(rbounds, fill);
            else
                canvas.drawRect(bounds, fill);
        }

        // 3. Border
        if (decoration.border.has_value())
        {
            const auto& b     = *decoration.border;
            const Paint stroke = Paint::stroked(b.color, b.width);
            if (has_radius)
                canvas.drawRRect(rbounds, stroke);
            else
                canvas.drawRect(bounds, stroke);
        }
    }

} // namespace systems::leal::campello_widgets
