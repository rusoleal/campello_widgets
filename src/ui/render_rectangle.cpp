#include <campello_widgets/ui/render_rectangle.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <cmath>

namespace systems::leal::campello_widgets
{

    void RenderRectangle::setColor(Color color) noexcept
    {
        color_ = color;
        markNeedsPaint();
    }

    void RenderRectangle::setFill(bool fill) noexcept
    {
        fill_ = fill;
        markNeedsLayout();
    }

    void RenderRectangle::setCornerRadius(float radius) noexcept
    {
        corner_radius_ = radius;
        markNeedsPaint();
    }

    void RenderRectangle::performLayout()
    {
        if (fill_)
        {
            const float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
            const float h = std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height;
            size_ = constraints_.constrain(Size{w, h});
        }
        else
        {
            size_ = constraints_.constrain(Size::zero());
        }
    }

    void RenderRectangle::performPaint(PaintContext& context, const Offset& offset)
    {
        const Rect rect = Rect::fromOffsetAndSize(offset, size_);
        context.canvas().drawRect(rect, Paint::filled(color_));
    }

} // namespace systems::leal::campello_widgets
