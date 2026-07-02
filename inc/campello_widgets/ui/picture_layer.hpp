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
     * command bakes absolute (non-transform-deferred) geometry — clip rects/
     * rrects/ovals/paths, backdrop-filter and shader-mask bounds, and
     * save-layer bounds. `Renderer::flushDrawList()` treats these
     * differently from ordinary draw geometry: `PushTransformCmd` composes
     * multiplicatively into the ambient transform
     * (`current_transform = current_transform * c.transform`), so draw
     * geometry recorded under it is safe to relocate by wrapping it in an
     * additional `Canvas::translate()` later. Clip commands are not —
     * `PushClipRectCmd` is a direct absolute reassignment
     * (`current_clip = c.rect`), never multiplied by the transform stack —
     * so a cached slice containing one cannot be safely repositioned by a
     * wrapping translate alone; it must be re-recorded at the new position
     * instead. `OffsetLayer` uses this flag to decide which path is safe.
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
