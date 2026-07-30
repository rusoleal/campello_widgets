#include <campello_widgets/ui/offset_layer.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <cstdio>

namespace systems::leal::campello_widgets
{

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
            // ClipRRect/ClipOval/ShaderMask inside is guaranteed unchanged
            // (this whole slice is a byte-for-byte copy of what was
            // recorded last time), and can safely reuse its cached GPU
            // composite instead of recapturing it. See
            // CacheReplayBeginCmd's doc comment. Only the identity-replay
            // path is bracketed — the delta-translate path below can never
            // carry clip-shape content in the first place, since
            // hasUnsafeGeometry() (which every clip/backdrop/shader-mask
            // sets) always forces a full re-record on reposition instead.
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

        // Cheap reposition: the cached content contains no absolute-baked
        // geometry, so wrapping it in a delta translate is equivalent to a
        // fresh recording at the new offset — see the class doc comment.
        canvas.save();
        canvas.translate(offset.x - recorded_offset_.x, offset.y - recorded_offset_.y);
        canvas.appendRecorded(picture_.commands());
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
