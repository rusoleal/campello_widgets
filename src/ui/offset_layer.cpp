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

        Canvas& canvas = context.canvas();

        if (offset == recorded_offset_)
        {
            // A backdrop filter's noteBackdropFilter() side effect must run
            // every frame this content paints, dirty or not, moved or not —
            // see the class doc comment. Never replay; always force a fresh
            // record. Nothing about this frame's on-screen bounds actually
            // changed (offset is unchanged), so this is *not* reported as
            // dirty — see the reposition branch below for the case where it
            // must be.
            if (picture_.hasBackdropFilter())
                return false;

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
            canvas.appendRecorded(DrawList{CacheReplayBeginCmd{this, incarnation_id_}});
            canvas.appendRecorded(picture_.commands());
            canvas.appendRecorded(DrawList{CacheReplayEndCmd{}});
            return true;
        }

        // Offset changed — content visibly moved either way (safe delta
        // translate below, or a forced full re-record for unsafe geometry
        // or a backdrop filter), so both the vacated and the
        // newly-occupied screen region changed. This must be reported
        // *before* the hasBackdropFilter()/hasUnsafeGeometry() early-outs
        // below: a moving BackdropFilter's old and new regions are exactly
        // what buildFrame() needs to know are dirty in order to decide the
        // backdrop capture can't be skipped this frame — skip the report
        // and the capture goes stale at the old scroll position while the
        // filter itself keeps painting (freshly, every frame) at the new
        // one.
        noteDirty(recorded_offset_, "reposition-old");
        noteDirty(offset, "reposition-new");

        // A backdrop filter forces a full re-record on every repaint (see
        // the identity-branch comment above) — moving is no exception.
        if (picture_.hasBackdropFilter())
            return false;

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
        canvas.appendRecorded(DrawList{CacheReplayBeginCmd{this, incarnation_id_}});
        canvas.appendRecorded(shiftClipRects(picture_.commands(), dx, dy));
        canvas.appendRecorded(DrawList{CacheReplayEndCmd{}});
        canvas.restore();
        return true;
    }

    void OffsetLayer::record(PaintContext& context, const Offset& offset,
                              const std::function<void()>& paintContent)
    {
        // A fresh record replaces picture_'s content outright, but any GPU
        // replay-cache entries (shadow/clip-shape/shader-mask/save-layer
        // textures) from a *previous* record are still sitting in the
        // Renderer's caches, keyed by (this pointer, bracket_index) — see
        // Renderer::evictReplayCacheEntries()'s doc. Those indices are
        // assigned purely by encounter-order within a replay, so if this
        // object records more than once in its lifetime (e.g. its content
        // shifted position between two genuinely-dirty repaints), the next
        // identity-replay of the new picture would silently reuse the OLD
        // record's cached texture/bounds under the same key — a real bug
        // found via a Card's box-shadow rendering at a stale position after
        // its container settled from an intermediate layout into its final
        // one. Evicting unconditionally here (not just in the destructor,
        // which only guards against address reuse across different
        // OffsetLayer instances) keeps that cache honest across repeated
        // records of the same instance too.
        if (auto* renderer = detail::currentRenderer().load(std::memory_order_acquire))
            renderer->evictReplayCacheEntries(this);

        picture_.record(context, paintContent);
        recorded_offset_ = offset;
        has_recorded_     = true;
    }

} // namespace systems::leal::campello_widgets
