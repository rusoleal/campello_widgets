#pragma once

#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Records a subtree's paint output as a standalone `DrawList`
     * slice, for `OffsetLayer` to replay.
     *
     * Also classifies the recording: `hasUnsafeGeometry()` is true if any
     * command bakes absolute geometry that its *sole* consumer uses via a
     * direct reassignment never multiplied by the ambient transform, and
     * that this class has no way to correct for a moved replay. Only two
     * commands qualify: `PushClipPathCmd` (`current_clip =
     * current_clip.intersection(c.path.getBounds())` — a direct-assignment
     * consumer like `PushClipRectCmd` below, but `Path` has no
     * `translate()` yet to shift it with) and `DrawBackdropFilterBeginCmd`
     * (see `hasBackdropFilter()` below — excluded here too for belt-and-
     * suspenders clarity, though `OffsetLayer` already special-cases it
     * before this flag is even consulted). `OffsetLayer` forces a full
     * re-record on reposition for these — see `maybeReplay()`.
     *
     * Everything else that bakes absolute geometry turns out *not* to need
     * that: `PushClipRectCmd` is a direct reassignment too
     * (`current_clip = c.rect`) but `OffsetLayer::maybeReplay()` shifts its
     * stored rect by hand when repositioning, since it's simple to. And
     * `PushClipRRectCmd`/`PushClipOvalCmd`/`DrawShaderMaskBeginCmd`/
     * `SaveLayerCmd`'s stored bounds all flow through exactly one
     * consumer — an offscreen-composite draw
     * (`drawClipShapeComposite`/`drawShaderMaskComposite`/
     * `saveLayerComposite`) that *does* multiply that bounds by the
     * ambient transform, the same mechanism that already makes ordinary
     * draw geometry safe to relocate by wrapping it in an additional
     * `Canvas::translate()` — so these need no special handling either.
     *
     * `hasBackdropFilter()` is a stricter, separate flag: a recording
     * containing `DrawBackdropFilterBeginCmd` must never be replayed at
     * all — not even an identity replay at the same offset — because
     * `RenderBackdropFilter::performPaint()` has a side effect
     * (`Renderer::noteBackdropFilter()`) that must fire every single frame
     * this content is visible, regardless of whether its own offset or
     * dirty state changed. That side effect is what keeps the Renderer's
     * full-viewport backdrop-capture pass running; skip `performPaint()`
     * via a cache replay and the capture pass silently stops updating,
     * leaving the blur sampling a stale, frozen backdrop — exactly the
     * "BackdropFilter inside a scroll doesn't respect offset" symptom this
     * flag exists to prevent (the destination quad still tracks the
     * ambient transform correctly; the *sampled content* goes stale
     * because the capture pass that produces it stopped running). See
     * `OffsetLayer::maybeReplay()`.
     */
    class PictureLayer
    {
    public:
        bool hasContent() const noexcept { return has_content_; }
        bool hasUnsafeGeometry() const noexcept { return has_unsafe_geometry_; }
        bool hasBackdropFilter() const noexcept { return has_backdrop_filter_; }
        const DrawList& commands() const noexcept { return commands_; }

        /**
         * @brief Records fresh content by invoking `paintContent` (which
         * must issue draw calls into `context`), then remembers the slice
         * of commands it produced.
         */
        void record(PaintContext& context, const std::function<void()>& paintContent);

    private:
        DrawList commands_;
        bool     has_content_         = false;
        bool     has_unsafe_geometry_ = false;
        bool     has_backdrop_filter_ = false;
    };

} // namespace systems::leal::campello_widgets
