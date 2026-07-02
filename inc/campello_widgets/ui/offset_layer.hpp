#pragma once

#include <campello_widgets/ui/picture_layer.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Positions a cached `PictureLayer`, replaying it when clean.
     *
     * Successor to `PaintCache`: a strict, provably-safe superset. Every
     * path `PaintCache` handled is unchanged (dirty/no-cache/constraints
     * change forces a full re-record; identity replay skips straight to
     * `appendRecorded()`) — the one new path is a *cheap reposition*: when
     * only the on-screen offset changed and the cached picture contains no
     * absolute-baked geometry (`PictureLayer::hasUnsafeGeometry()`), the
     * cached commands are replayed under an additional delta
     * `Canvas::translate()` instead of being re-recorded from scratch. This
     * is safe because ordinary draw-command geometry is transform-deferred —
     * `Renderer::flushDrawList()` composes `PushTransformCmd`s
     * multiplicatively into the ambient transform before applying it to
     * vertices — so wrapping unchanged, transform-safe content in one more
     * translate reproduces exactly what a fresh recording at the new offset
     * would have produced. When the picture contains unsafe geometry (a
     * clip, backdrop filter, or shader mask — all of which bake absolute,
     * non-transform-deferred bounds/rects), this falls back to a full
     * re-record at the new offset, identical to `PaintCache`'s behavior.
     *
     * A picture containing a backdrop filter is never replayed at all —
     * not even an identity replay at an unchanged offset — see
     * `PictureLayer::hasBackdropFilter()`'s doc comment for why: its
     * `noteBackdropFilter()` side effect must fire every frame this
     * content is visible, or the Renderer's backdrop-capture pass silently
     * stops updating and the blur goes stale.
     */
    class OffsetLayer
    {
    public:
        /**
         * @brief Attempts to replay the cache. Returns true if it did (the
         * caller must not record anything else this frame); false if the
         * cache is missing, dirty, or the offset changed and the picture
         * contains unsafe geometry — the caller must call `record()`.
         *
         * `size` is the caller's own on-screen size, used only to report
         * dirty regions (via `Renderer::noteDirtyRegion()`) when the cheap
         * delta-translate reposition path fires: content visibly changed
         * at both the old and new screen position even though nothing was
         * marked `needs_paint_` — see `Renderer::noteDirtyRegion()`'s doc.
         */
        bool maybeReplay(PaintContext& context, const Offset& offset,
                         const Size& size, bool dirty);

        /**
         * @brief Records fresh content at `offset` by invoking
         * `paintContent`, then remembers the offset it was recorded at.
         */
        void record(PaintContext& context, const Offset& offset,
                    const std::function<void()>& paintContent);

    private:
        PictureLayer picture_;
        Offset       recorded_offset_{};
        bool         has_recorded_ = false;
    };

} // namespace systems::leal::campello_widgets
