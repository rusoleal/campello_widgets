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
        const bool offset_changed = has_cache_ && !(offset == cached_offset_);

        if (needsPaint() || !has_cache_ || offset_changed)
        {
            // Dirty, first paint ever, or repositioned since the cache was
            // recorded — paint the child normally, directly into the live
            // context, then remember which slice of the resulting DrawList
            // it produced. Recording into the *live* context (rather than a
            // separate local/headless one) matters: PushClipRectCmd bakes an
            // absolute rect at record time (unlike draw-command geometry,
            // which is transform-deferred to flush time — see
            // flushDrawList()'s `current_clip = c.rect;`), so a clip
            // recorded against a fabricated local origin would replay at the
            // wrong position once translated. Recording at the real
            // `offset` keeps every command — including clips — correct for
            // *this* offset, with nothing to re-derive on replay.
            const size_t start = context.canvas().commands().size();
            paintChild(context, offset);
            const DrawList& all = context.canvas().commands();
            cached_commands_.assign(all.begin() + static_cast<std::ptrdiff_t>(start), all.end());
            has_cache_     = true;
            cached_offset_ = offset;
            needs_paint_   = false;
            return;
        }

        // Clean and at the same offset as last recorded — replay the cached
        // slice instead of walking the child.
        context.canvas().appendRecorded(cached_commands_);
    }

} // namespace systems::leal::campello_widgets
