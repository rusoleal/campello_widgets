#pragma once

#include <memory>
#include <vector>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/text_span.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief Recording canvas passed to RenderObject::performPaint().
     *
     * Canvas accumulates draw commands (drawRect, drawText, drawImage) and
     * transform/clip state into a DrawList that the Renderer flushes to the
     * GPU backend after the paint pass.
     *
     * State is managed Flutter-style: `save()` snapshots both the current
     * transform and clip together; `restore()` rolls them back atomically,
     * emitting the matching Pop commands into the DrawList.
     *
     * **Coordinate system:** origin (0, 0) is the top-left corner of the
     * viewport, x increases right, y increases down.
     *
     * **Thread safety:** Canvas is not thread-safe. Use it only on the
     * render thread.
     */
    class Canvas
    {
    public:
        Canvas(float viewport_width, float viewport_height);

        // ------------------------------------------------------------------
        // Draw primitives
        // ------------------------------------------------------------------

        /** @brief Fills or strokes an axis-aligned rectangle. */
        void drawRect(const Rect& rect, const Paint& paint);

        /**
         * @brief Draws a text span with its top-left corner at `origin`.
         *
         * @param span   Text content and style.
         * @param origin Top-left corner in local coordinates.
         */
        void drawText(const TextSpan& span, const Offset& origin);

        /**
         * @brief Draws a GPU texture into a destination rectangle.
         *
         * @param texture  Source texture.
         * @param src_rect Normalised source rect [0,1]×[0,1].
         *                 Pass Rect::fromLTWH(0,0,1,1) for the full texture.
         * @param dst_rect Destination in local coordinates.
         */
        void drawImage(
            std::shared_ptr<campello_gpu::Texture> texture,
            const Rect& src_rect,
            const Rect& dst_rect);

        // ------------------------------------------------------------------
        // State management
        // ------------------------------------------------------------------

        /**
         * @brief Saves the current transform, clip, and opacity onto the save stack.
         *
         * Must be balanced by a matching `restore()`. Transforms, clips, and
         * opacity changes applied after `save()` are unwound when `restore()` is called.
         */
        void save();

        /**
         * @brief Restores the transform and clip saved by the matching `save()`.
         *
         * Emits the necessary PopTransform and PopClipRect commands into the
         * DrawList to mirror the state changes made since the matching save.
         */
        void restore();

        // ------------------------------------------------------------------
        // Transform modifiers
        // ------------------------------------------------------------------

        /**
         * @brief Post-multiplies a translation onto the current transform.
         *
         * Equivalent to `transform(Matrix4::translate({dx, dy, 0.0f}))`.
         */
        void translate(float dx, float dy);

        /**
         * @brief Post-multiplies `matrix` onto the current transform.
         *
         * Emits a PushTransformCmd. Must be within a save/restore pair so the
         * push is eventually popped; otherwise use `save()`/`restore()` to
         * scope the transform automatically.
         */
        void transform(const Matrix4& matrix);

        // ------------------------------------------------------------------
        // Clip
        // ------------------------------------------------------------------

        /**
         * @brief Intersects `rect` with the current clip and pushes the result.
         *
         * Emits a PushClipRectCmd. Must be within a save/restore pair.
         */
        void clipRect(const Rect& rect);

        // ------------------------------------------------------------------
        // Opacity
        // ------------------------------------------------------------------

        /**
         * @brief Multiplies `factor` into the current effective opacity.
         *
         * The resulting opacity is baked into every subsequent draw call's
         * colour channel until `restore()` reverts it. Nesting two `setOpacity`
         * calls inside separate save/restore scopes composes them multiplicatively:
         * `0.5` inside `0.8` yields an effective opacity of `0.4`.
         *
         * Must be called inside a matching `save()`/`restore()` pair so the
         * previous opacity is automatically restored.
         *
         * @param factor Opacity factor in [0, 1].
         */
        void setOpacity(float factor) noexcept;

        // ------------------------------------------------------------------
        // Accessors
        // ------------------------------------------------------------------

        /** @brief The current effective transform (product of all live pushes). */
        const Matrix4& currentTransform() const noexcept { return current_transform_; }

        /**
         * @brief The current effective clip (intersection of all live clips).
         *
         * Returns a full-viewport rect when no clip is active.
         */
        const Rect& currentClip() const noexcept { return current_clip_; }

        /** @brief The current effective opacity (product of all nested setOpacity calls). */
        float currentOpacity() const noexcept { return current_opacity_; }

        /** @brief The accumulated draw commands recorded so far. */
        const DrawList& commands() const noexcept { return commands_; }

    private:
        // Tracks how many Push commands were emitted inside a save() scope so
        // that restore() can emit the exact number of Pop commands.
        struct SaveEntry
        {
            Matrix4 transform;
            Rect    clip;
            float   opacity           = 1.0f;
            int     pushed_transforms = 0;
            int     pushed_clips      = 0;
        };

        DrawList               commands_;
        Matrix4                current_transform_;
        Rect                   current_clip_;
        float                  current_opacity_ = 1.0f;
        std::vector<SaveEntry> save_stack_;
    };

} // namespace systems::leal::campello_widgets
