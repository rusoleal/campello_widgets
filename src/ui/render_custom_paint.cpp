#include <campello_widgets/ui/render_custom_paint.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <cmath>

namespace systems::leal::campello_widgets
{

    void RenderCustomPaint::setPainter(
        std::shared_ptr<CustomPainter> painter) noexcept
    {
        const bool needs_repaint = !painter_ ||
            !painter ||
            painter->shouldRepaint(*painter_);

        prev_painter_ = std::move(painter_);
        painter_      = std::move(painter);

        if (needs_repaint)
            markNeedsPaint();
    }

    void RenderCustomPaint::performLayout()
    {
        const float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
        const float h = std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height;
        size_ = constraints_.constrain(Size{w, h});
    }

    void RenderCustomPaint::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!painter_) return;

        Canvas& canvas = context.canvas();
        canvas.save();
        canvas.translate(offset.x, offset.y);
        painter_->paint(canvas, size_);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
