#include <campello_widgets/ui/render_backdrop_filter.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/hit_test.hpp>

namespace systems::leal::campello_widgets
{

    void RenderBackdropFilter::performLayout()
    {
        // Notify the Renderer that a BackdropFilter exists this frame.
        if (auto* r = detail::currentRenderer())
            r->noteBackdropFilter(filter_);

        // Lay out the child (if any) tightly to our constraints.
        if (child_)
        {
            child_->layout(constraints());
            size_ = child_->size();
        }
        else
        {
            const auto& c = constraints();
            size_ = Size{c.max_width, c.max_height};
        }
    }

    void RenderBackdropFilter::performPaint(PaintContext& ctx, const Offset& offset)
    {
        const Rect bounds = Rect::fromLTWH(
            offset.x, offset.y, size_.width, size_.height);

        // Begin backdrop filter scope — wraps children in the draw list.
        ctx.canvas().beginBackdropFilter(bounds, filter_);

        if (child_)
            paintChild(ctx, offset);

        ctx.canvas().endBackdropFilter();
    }

    bool RenderBackdropFilter::hitTestChildren(
        HitTestResult& result, const Offset& position)
    {
        if (child_)
            return child_->hitTest(result, position);
        return false;
    }

} // namespace systems::leal::campello_widgets
