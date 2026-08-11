#include <campello_widgets/ui/offset_layer.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <cstdio>
#include <type_traits>
#include <variant>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // Shifts PushClipRectCmd's stored rect by (dx,dy) — see
        // PictureLayer's doc comment for why it's the one command among a
        // cheap-repositioned picture's contents whose sole consumer
        // (`current_clip = c.rect`) never re-applies the ambient transform,
        // so a replayed copy needs its absolute geometry corrected by hand.
        // Everything else passes through unchanged.
        DrawList shiftClipRects(const DrawList& commands, float dx, float dy)
        {
            DrawList shifted;
            shifted.reserve(commands.size());
            for (const auto& cmd : commands)
            {
                std::visit([&](auto arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, PushClipRectCmd>)
                    {
                        arg.rect.x += dx;
                        arg.rect.y += dy;
                    }
                    shifted.push_back(arg);
                }, cmd);
            }
            return shifted;
        }
    }

    OffsetLayer::~OffsetLayer()
    {
        if (auto* renderer = detail::currentRenderer().load(std::memory_order_acquire))
            renderer->evictReplayCacheEntries(this);
    }

    bool OffsetLayer::maybeReplay(PaintContext& context, const Offset& offset,
                                   const Size& size, bool own_dirty, bool descendant_dirty)
    {
        auto* renderer = detail::currentRenderer().load(std::memory_order_acquire);
        // Projected through the ambient transform (Transform widgets, or a
        // scroll's canvas.translate()) — offset-based positioning alone
        // isn't the true on-screen position once any such transform is
        // active. See projectedBounds()'s doc.
        const Matrix4& ambient_transform = context.canvas().currentTransform();
        auto  noteDirty = [&](const Offset& o, const char* reason) {
            const Rect bounds = projectedBounds(ambient_transform, Rect::fromOffsetAndSize(o, size));
            if (renderer) renderer->noteDirtyRegion(bounds);
            if (DebugFlags::printDirtyRegionTrace)
                std::fprintf(stderr,
                    "[dirty] OffsetLayer(%s) (%.0f,%.0f %.0fx%.0f)\n",
                    reason, bounds.x, bounds.y, bounds.width, bounds.height);
        };

        // The classes that own an OffsetLayer (RenderRepaintBoundary and the
        // self-boundaring scrollables) override RenderObject::paint() rather
        // than going through its base implementation, so they never get the
        // dirty-region report that base paint() gives every other node (see
        // Renderer::noteDirtyRegion()'s doc). This is the one place all of
        // them funnel through, so it's handled centrally here instead of
        // once per caller: any time this function is about to trigger a
        // real re-record — because content is genuinely dirty, or because
        // it moved (whether via a cheap delta-translate or a forced full
        // re-record for unsafe geometry) — report the affected bounds.
        // The one exception is the backdrop-filter safety re-record below:
        // that one is a pure correctness precaution with nothing actually
        // changed, so it must NOT be reported as dirty, or it would defeat
        // the very optimization this mechanism exists for.
        //
        // own_dirty and descendant_dirty both force the same real-record
        // fallback, but only own_dirty is reported via noteDirty() — see
        // maybeReplay()'s doc comment in offset_layer.hpp for why folding
        // descendant_dirty in here too would spuriously widen this frame's
        // dirty region to this whole boundary's bounds.
        if (own_dirty || descendant_dirty || !has_recorded_ || !picture_.hasContent())
        {
            if (own_dirty) noteDirty(offset, "dirty");
            return false;
        }

        // A backdrop filter's noteBackdropFilter() side effect must run
        // every frame this content paints, dirty or not, moved or not — see
        // the class doc comment. Never replay; always force a fresh record.
        if (picture_.hasBackdropFilter())
            return false;

        Canvas& canvas = context.canvas();

        if (offset == recorded_offset_)
        {
            // Bracket the replay with CacheReplayBeginCmd/EndCmd, carrying
            // this OffsetLayer's own address as an opaque identity —
            // Renderer::flushDrawList() uses it to recognize that any
            // ClipRRect/ClipOval/ShaderMask/SaveLayer/shadow inside is
            // guaranteed unchanged content (this whole slice is a
            // byte-for-byte copy of what was recorded last time — only its
            // on-screen position may differ, see the delta-translate branch
            // below for why that's still safe to bracket too), and can
            // safely reuse its cached GPU composite instead of recapturing
            // it. See CacheReplayBeginCmd's doc comment.
            canvas.appendRecorded(DrawList{CacheReplayBeginCmd{this}});
            canvas.appendRecorded(picture_.commands());
            canvas.appendRecorded(DrawList{CacheReplayEndCmd{}});
            return true;
        }

        // Offset changed — content visibly moved either way (safe delta
        // translate below, or a forced full re-record for unsafe geometry),
        // so both the vacated and the newly-occupied screen region changed.
        noteDirty(recorded_offset_, "reposition-old");
        noteDirty(offset, "reposition-new");

        if (picture_.hasUnsafeGeometry())
            return false;

        // Cheap reposition: wrapping the cached content in a delta
        // translate is equivalent to a fresh recording at the new offset —
        // see the class doc comment for why that's true even for the
        // absolute-baked geometry PictureLayer no longer flags as unsafe.
        // shiftClipRects() handles the one exception (PushClipRectCmd) by
        // hand; everything else needs no adjustment beyond the translate.
        const float dx = offset.x - recorded_offset_.x;
        const float dy = offset.y - recorded_offset_.y;

        canvas.save();
        canvas.translate(dx, dy);
        // Bracketed with CacheReplayBeginCmd/EndCmd for the same reason as
        // the identity-replay branch above: this content is still
        // guaranteed unchanged from what was last recorded (only *where*
        // it's drawn moved) — and every GPU-cacheable consumer
        // (drawClipShapeComposite/drawShaderMaskComposite/
        // saveLayerComposite/the shadow composite) already re-applies the
        // ambient transform — which now includes this translate — to its
        // bounds on every use, cache hit or not. So a cache hit here still
        // composites at the correct, moved position; this bracket is
        // purely an opt-in performance signal, not a correctness
        // requirement of the reposition itself.
        canvas.appendRecorded(DrawList{CacheReplayBeginCmd{this}});
        canvas.appendRecorded(shiftClipRects(picture_.commands(), dx, dy));
        canvas.appendRecorded(DrawList{CacheReplayEndCmd{}});
        canvas.restore();
        return true;
    }

    void OffsetLayer::record(PaintContext& context, const Offset& offset,
                              const std::function<void()>& paintContent)
    {
        picture_.record(context, paintContent);
        recorded_offset_ = offset;
        has_recorded_     = true;
    }

} // namespace systems::leal::campello_widgets
