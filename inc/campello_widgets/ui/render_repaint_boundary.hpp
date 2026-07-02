#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/offset_layer.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Caches its child's paint output so an unrelated repaint elsewhere
     * in the tree doesn't force this subtree to be re-walked.
     *
     * Layout is pass-through, identical to RenderClipRect: the child receives
     * this box's constraints and this box takes the child's size.
     *
     * Paint is where this differs from every other RenderBox: when this node
     * is clean (`needsPaint() == false`), its on-screen offset hasn't changed
     * since the cache was recorded, and a cache already exists, `paint()`
     * skips `performPaint()`/the child subtree entirely and replays the cached
     * `DrawList` slice instead. This is the only RenderObject in the tree
     * that overrides `paint()` itself rather than just `performPaint()` —
     * see `RenderObject::paint()`'s doc comment for why: a clean subtree
     * still needs *something* appended to this frame's DrawList, so skipping
     * outright isn't an option; a cached replay is.
     *
     * The cache is recorded by painting the child into the *live* context at
     * its real on-screen offset (not a fabricated local origin) and
     * remembering which slice of commands that produced — clip rects are
     * baked to absolute coordinates at record time (unlike draw-command
     * geometry, which defers its transform to flush time), so a clip
     * recorded against a local origin and later translated on replay would
     * land in the wrong place. Recording at the real offset sidesteps that
     * entirely, at the cost of an assumption: this boundary's on-screen
     * offset must stay constant between the recording and a clean replay.
     * Ordinary repositioning (e.g. a `Row`/`Column` child shifting when a
     * sibling resizes, or `positionChild()` as used by `Stack`/`Positioned`)
     * deliberately doesn't mark needs-paint on its own — that's correct for
     * every other RenderObject, which always repaints at the fresh offset
     * regardless — so this class tracks the offset it was last painted at
     * itself and treats a change as dirty, rather than relying on every
     * caller to remember not to reposition it without also repainting it.
     *
     * Mirrors Flutter's `RepaintBoundary`, minus layer/texture compositing —
     * here "the layer" is just a cached `DrawList` slice, which is cheap to
     * copy since this framework has no GPU-side retained layers yet.
     *
     * The caching mechanism itself lives in `OffsetLayer` (composing a
     * `PictureLayer`) so other RenderObjects can self-boundary the same way
     * without being a RenderRepaintBoundary (e.g. scrollable viewports
     * mirroring Flutter's `RenderViewport.isRepaintBoundary`) — see its
     * class doc for the mechanism; this comment is the canonical
     * explanation of *why*.
     */
    class RenderRepaintBoundary : public RenderBox
    {
    public:
        void performLayout() override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return true; }

    private:
        OffsetLayer offset_layer_;
    };

} // namespace systems::leal::campello_widgets
