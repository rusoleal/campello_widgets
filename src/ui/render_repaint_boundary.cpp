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
        if (needsPaint() || !has_cache_)
        {
            // Dirty (or first paint ever) — paint the child normally, directly
            // into the live context, then remember which slice of the
            // resulting DrawList it produced. Recording into the *live*
            // context (rather than a separate local/headless one) matters:
            // PushClipRectCmd bakes an absolute rect at record time (unlike
            // draw-command geometry, which is transform-deferred to flush
            // time — see flushDrawList()'s `current_clip = c.rect;`), so a
            // clip recorded against a fabricated local origin would replay
            // at the wrong position once translated. Recording at the real
            // `offset` keeps every command — including clips — correct for
            // *this* offset, with nothing to re-derive on replay.
            const size_t start = context.canvas().commands().size();
            paintChild(context, offset);
            const DrawList& all = context.canvas().commands();
            cached_commands_.assign(all.begin() + static_cast<std::ptrdiff_t>(start), all.end());
            has_cache_   = true;
            needs_paint_ = false;
            return;
        }

        // Clean — replay the cached slice instead of walking the child.
        // This assumes `offset` is unchanged since the cache was recorded:
        // `positionChild()` (used by e.g. Stack/Positioned) deliberately
        // does not mark needs-paint on a pure reposition, so a parent that
        // moves this boundary without otherwise dirtying it would replay
        // stale geometry. Fine for a boundary with a fixed on-screen
        // position (the common case); don't wrap something that's
        // repositioned without a corresponding repaint.
        context.canvas().appendRecorded(cached_commands_);
    }

} // namespace systems::leal::campello_widgets
