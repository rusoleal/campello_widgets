#include <campello_widgets/ui/render_backdrop_filter.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/hit_test.hpp>

namespace systems::leal::campello_widgets
{

    void RenderBackdropFilter::performLayout()
    {
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
        // Logical (untransformed) bounds — this is what the draw command
        // carries. It's correctly projected through the ambient transform
        // later, at flush time (Renderer::flushDrawList() composes
        // PushTransformCmds into current_transform and the Metal backend's
        // drawBackdropFilter() applies it to these corners) — do not
        // change this to a pre-projected rect, or it would be transformed
        // twice.
        const Rect bounds = Rect::fromLTWH(
            offset.x, offset.y, size_.width, size_.height);

        // Notify the Renderer that a BackdropFilter exists this frame. Done
        // here rather than in performLayout(): layout short-circuits when
        // constraints are unchanged and the subtree isn't dirty (e.g. a
        // scroll-only repaint), which would silently skip the Renderer's
        // backdrop-capture pass and leave it compositing a stale texture.
        // Paint always re-runs for every visible frame, so this stays
        // accurate regardless of whether layout ran.
        //
        // The dirty-region gating check (see Renderer::noteDirtyRegion()'s
        // doc) needs the *true* on-screen bounds, not the logical `bounds`
        // above — offset-based positioning and any ambient canvas
        // transform (notably a scroll's canvas.translate(), which never
        // touches `offset` — see RenderSingleChildScrollView::
        // performPaint()) are independent and additive. Using the
        // unprojected `bounds` here would compare this filter's logical,
        // never-changing-while-scrolled position against other reporters'
        // true screen positions — silently breaking the capture-skip
        // decision for any BackdropFilter living inside a scrollable.
        if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
            r->noteBackdropFilter(projectedBounds(ctx.canvas().currentTransform(), bounds), filter_);

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
