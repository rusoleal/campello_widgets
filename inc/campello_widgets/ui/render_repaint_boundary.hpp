#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/draw_command.hpp>

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
     * is clean (`needsPaint() == false`) and a cache already exists, `paint()`
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
     * entirely, at the cost of an assumption: **this boundary's on-screen
     * offset must stay constant between the recording and a clean replay**.
     * `positionChild()` (used by `Stack`/`Positioned`) deliberately doesn't
     * mark needs-paint on a pure reposition, so don't wrap something whose
     * position changes without also dirtying it.
     *
     * Mirrors Flutter's `RepaintBoundary`, minus layer/texture compositing —
     * here "the layer" is just a cached `DrawList` slice, which is cheap to
     * copy since this framework has no GPU-side retained layers yet.
     */
    class RenderRepaintBoundary : public RenderBox
    {
    public:
        void performLayout() override;
        void paint(PaintContext& context, const Offset& offset) override;

    private:
        DrawList cached_commands_;
        bool     has_cache_ = false;
    };

} // namespace systems::leal::campello_widgets
