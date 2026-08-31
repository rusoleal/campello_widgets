#pragma once

#include <memory>
#include <vector>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/vertices.hpp>
#include <campello_widgets/ui/rs_transform.hpp>

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

        /** @brief Draws a circle with the given center and radius. */
        void drawCircle(const Offset& center, float radius, const Paint& paint);

        /** @brief Draws an oval that fills the given rectangle. */
        void drawOval(const Rect& rect, const Paint& paint);

        /**
         * @brief Draws an arc.
         * 
         * @param rect        The bounding rectangle of the full oval
         * @param start_angle Start angle in radians (0 = 3 o'clock)
         * @param sweep_angle Sweep angle in radians (positive = clockwise)
         * @param use_center  If true, draws a pie wedge including center
         * @param paint       The paint to use
         */
        void drawArc(const Rect& rect, float start_angle, float sweep_angle, 
                     bool use_center, const Paint& paint);

        /** @brief Draws a line between two points. */
        void drawLine(const Offset& p1, const Offset& p2, const Paint& paint);

        /** @brief Draws a rounded rectangle. */
        void drawRRect(const RRect& rrect, const Paint& paint);

        /** @brief Draws the difference of two rounded rectangles. */
        void drawDRRect(const RRect& outer, const RRect& inner, const Paint& paint);

        /** @brief Draws a path. */
        void drawPath(const Path& path, const Paint& paint);

        /** @brief Draws points according to the given mode. */
        void drawPoints(PointMode mode, const std::vector<Offset>& points, const Paint& paint);

        /**
         * @brief Draws an arbitrary triangle mesh -- matches Flutter's
         * `Canvas.drawVertices(vertices, blendMode, paint)`.
         *
         * `blend_mode` combines `paint.color` (as Porter-Duff "src") with
         * each vertex's own color (as "dst"), the same formula
         * `Paint.colorFilter` uses -- computed once per input vertex on the
         * CPU, then linearly interpolated across each triangle by the GPU.
         * This is mathematically exact whenever a triangle's vertex alphas
         * are equal (the common case -- most meshes use fully-opaque
         * per-vertex colors), and for `srcIn`/`srcOut`/`src`/`dst`/`clear`/
         * `srcOver`/`modulate` regardless of alpha variation; for other
         * modes combined with varying per-vertex alpha within one triangle,
         * it's a smooth-interpolation approximation of the true per-pixel
         * blend rather than an exact match.
         *
         * `paint.shader`, if set, paints the whole mesh with the shader
         * (via the same shader-mask mechanism every other `drawXxx()` uses)
         * instead of a flat/vertex-blended color -- e.g. a gradient-shaded
         * mesh. Out of scope: `vertices`' own texture coordinates (a
         * shader sampled *per vertex UV*, for texture-mapped mesh warping)
         * and mesh-edge antialiasing (triangles are flat/interpolated-color
         * with no AA skirt).
         */
        void drawVertices(const Vertices& vertices, BlendMode blend_mode, const Paint& paint);

        /**
         * @brief Draws a shadow for a path.
         * 
         * @param path                The path casting the shadow
         * @param color               The shadow color
         * @param elevation           The material elevation
         * @param transparent_occluder Whether the path is transparent
         */
        void drawShadow(const Path& path, const Color& color, float elevation, 
                        bool transparent_occluder);

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
         * @param texture       Source texture.
         * @param src_rect      Normalised source rect [0,1]×[0,1].
         *                      Pass Rect::fromLTWH(0,0,1,1) for the full texture.
         * @param dst_rect      Destination in local coordinates.
         * @param filter_quality Sampling quality (Flutter's `Paint.filterQuality`,
         *                      exposed as a direct parameter since this draw
         *                      doesn't take a full `Paint`); `none` selects
         *                      nearest-neighbor, anything else bilinear —
         *                      see `FilterQuality`'s doc comment.
         */
        void drawImage(
            std::shared_ptr<campello_gpu::Texture> texture,
            const Rect& src_rect,
            const Rect& dst_rect,
            FilterQuality filter_quality = FilterQuality::high);

        /**
         * @brief Draws a 9-slice-scaled texture: `center` splits it into a
         * 3x3 grid of patches, the 4 corners keep their source pixel size
         * unscaled, the 4 edges stretch along one axis, and the middle
         * patch stretches along both -- the classic 9-patch/border-image
         * technique for stretchy UI chrome (rounded panels, speech
         * bubbles, ...) without visibly distorting the corners. Mirrors
         * Flutter's `Canvas.drawImageNine()`.
         *
         * Pure CPU geometry on top of the existing `drawImage()` -- issues
         * up to 9 ordinary `drawImage()` calls (fewer if a patch is
         * degenerate, e.g. `center` touches an edge of the image), no new
         * backend/shader work.
         *
         * @param texture  Source texture.
         * @param center   The stretchable middle region, in *source pixel*
         *                 coordinates (matching Flutter's convention --
         *                 not normalised [0,1] like `drawImage()`'s
         *                 `src_rect`). Clamped to the texture's own bounds.
         * @param dst_rect Destination in local coordinates.
         * @param filter_quality Sampling quality -- see `drawImage()`.
         *
         * If `dst_rect` is smaller than the sum of the (unscaled) corner
         * sizes on either axis, each corner's destination size is
         * independently clamped to half of `dst_rect`'s size on that axis
         * -- correct for the common case (`dst_rect` at least as large as
         * the corners) but not a full proportional-shrink of every fixed
         * region the way some other 9-patch implementations handle
         * extreme over-squeezing.
         */
        void drawImageNine(
            std::shared_ptr<campello_gpu::Texture> texture,
            const Rect& center,
            const Rect& dst_rect,
            FilterQuality filter_quality = FilterQuality::high);

        /**
         * @brief Draws many sub-regions of one atlas texture, each
         * independently rotated/scaled/translated -- matches Flutter's
         * `Canvas.drawAtlas()`, scoped to its `atlas`/`transforms`/`rects`
         * parameters only.
         *
         * `transforms[i]` positions `rects[i]` (an atlas-pixel-space
         * sub-rect, *not* normalised — unlike `drawImage()`'s `src_rect`)
         * so that sprite's own local `[0,rects[i].width] x
         * [0,rects[i].height]` box lands wherever the `RSTransform` maps
         * it. Implemented as one `save()`/`transform()`/`drawImage()`/
         * `restore()` sequence per sprite — pure CPU geometry reusing
         * existing Canvas primitives, no new GPU work.
         *
         * Out of scope (see `RSTransform`'s doc comment / this method's
         * implementation notes for why): per-sprite `colors`/`blendMode`
         * tinting (would need real per-fragment blending against the
         * sampled atlas content — a new instanced GPU pipeline, not
         * attempted here) and `cullRect` (a pure performance hint with no
         * effect without batching, so dropped rather than kept as a
         * no-op). `transforms.size()` must equal `rects.size()`.
         */
        void drawAtlas(
            std::shared_ptr<campello_gpu::Texture> atlas,
            const std::vector<RSTransform>& transforms,
            const std::vector<Rect>& rects,
            FilterQuality filter_quality = FilterQuality::high);

        /**
         * @brief Draws a "template" texture recolored to an arbitrary tint.
         *
         * The source texture's own RGB is ignored; only its alpha channel
         * is sampled, used as a stencil for `tint` — see
         * `DrawTintedImageCmd`'s doc comment. Used by the `Icon` widget.
         *
         * @param texture  Source (template) texture.
         * @param src_rect Normalised source rect [0,1]×[0,1].
         * @param dst_rect Destination in local coordinates.
         * @param tint     Straight-alpha recolor applied via the texture's alpha.
         * @param filter_quality Sampling quality -- see `drawImage()`.
         */
        void drawTintedImage(
            std::shared_ptr<campello_gpu::Texture> texture,
            const Rect& src_rect,
            const Rect& dst_rect,
            const Color& tint,
            FilterQuality filter_quality = FilterQuality::high);

        /** @brief Fills the canvas with the given paint. */
        void drawPaint(const Paint& paint);

        /** @brief Fills the canvas with the given color using a blend mode. */
        void drawColor(const Color& color, BlendMode blend_mode);

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
         * @brief Saves and creates a new compositing layer.
         * 
         * This is similar to save() but also creates an offscreen buffer that
         * subsequent draw calls will render into. When restore() is called,
         * the layer is composited with the given paint's blend mode and color filter.
         * 
         * @param bounds Optional bounds for the layer (empty = use current clip).
         * @param paint  Paint containing blend mode and color filter.
         */
        void saveLayer(const Rect& bounds, const Paint& paint);

        /**
         * @brief Restores the transform and clip saved by the matching `save()`.
         *
         * Emits the necessary PopTransform and PopClipRect commands into the
         * DrawList to mirror the state changes made since the matching save.
         */
        void restore();

        /**
         * @brief Restores to a specific save count.
         * 
         * @param count The target save count (from getSaveCount).
         */
        void restoreToCount(int count);

        /**
         * @brief Returns the current save count.
         * 
         * The save count starts at 1 for a clean canvas. Each save() or saveLayer()
         * increments it, and restore() decrements it.
         */
        int getSaveCount() const;

        /**
         * @brief Returns the current transform matrix.
         */
        Matrix4 getTransform() const { return current_transform_; };

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
         * @brief Post-multiplies a rotation onto the current transform.
         * 
         * @param radians Angle in radians clockwise.
         */
        void rotate(float radians);

        /**
         * @brief Post-multiplies a scale onto the current transform.
         * 
         * @param sx Horizontal scale factor.
         * @param sy Vertical scale factor (defaults to sx if not specified).
         */
        void scale(float sx, float sy);
        void scale(float s) { scale(s, s); }

        /**
         * @brief Post-multiplies a skew onto the current transform.
         * 
         * @param sx Horizontal skew in radians.
         * @param sy Vertical skew in radians.
         */
        void skew(float sx, float sy);

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

        /**
         * @brief Intersects `rrect` with the current clip.
         */
        void clipRRect(const RRect& rrect);

        /**
         * @brief Clips to the oval inscribed in `rect`, intersected with the
         *        current clip's bounding box.
         *
         * Emits a PushClipOvalCmd. Must be within a save/restore pair.
         */
        void clipOval(const Rect& rect);

        /**
         * @brief Intersects `path` with the current clip.
         */
        void clipPath(const Path& path);

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
         * Returns a full-viewport rect when no clip is active. Tracked in
         * absolute/destination space (see `getDestinationClipBounds()`).
         */
        const Rect& currentClip() const noexcept { return current_clip_; }

        /**
         * @brief The current effective clip in destination (device/root)
         * space, i.e. after `currentTransform()` has been applied.
         *
         * This is simply `currentClip()` under another name — clip bounds
         * are tracked in absolute space internally — kept as a distinct
         * accessor for parity with Flutter's `Canvas.getDestinationClipBounds()`.
         */
        const Rect& getDestinationClipBounds() const noexcept { return current_clip_; }

        /**
         * @brief The current effective clip, mapped back into the caller's
         * local (pre-transform) coordinate space.
         *
         * Inverts `currentTransform()` against `currentClip()`'s four
         * corners and re-bounds them into an axis-aligned rect, so a
         * rotated/skewed ambient transform still yields a conservative
         * (possibly larger) local-space bound rather than an incorrect one.
         */
        Rect getLocalClipBounds() const noexcept;

        /** @brief The current effective opacity (product of all nested setOpacity calls). */
        float currentOpacity() const noexcept { return current_opacity_; }

        /** @brief The accumulated draw commands recorded so far. */
        const DrawList& commands() const noexcept { return commands_; }

        /**
         * @brief Appends a previously-recorded DrawList verbatim.
         *
         * Used by `RenderRepaintBoundary` to replay a cached subtree's
         * commands (recorded in its own local coordinate space via a separate
         * headless `PaintContext`) into the live recording, without re-walking
         * the subtree. Callers are responsible for bracketing with
         * `save()`/`translate()`/`restore()` so the replayed commands land at
         * the correct current offset.
         */
        void appendRecorded(const DrawList& recorded)
        {
            commands_.insert(commands_.end(), recorded.begin(), recorded.end());
        }

        // ------------------------------------------------------------------
        // BackdropFilter scope
        // ------------------------------------------------------------------

        /**
         * @brief Opens a backdrop-filter scope for `bounds` with `filter`.
         *
         * Draw commands issued after this call (until `endBackdropFilter()`)
         * are treated as the filter's children and will render on top of the
         * blurred backdrop.
         *
         * Must be balanced by a matching `endBackdropFilter()`.
         */
        void beginBackdropFilter(const Rect& bounds, const ImageFilter& filter);

        /** @brief Closes a backdrop-filter scope opened with `beginBackdropFilter()`. */
        void endBackdropFilter();

        // ------------------------------------------------------------------
        // ShaderMask scope
        // ------------------------------------------------------------------

        /**
         * @brief Opens a shader-mask scope for `bounds`.
         *
         * Draw commands issued after this call (until `endShaderMask()`)
         * are the mask's children; they are composited with the evaluated
         * `shader` using `blend_mode` before being drawn to the target.
         *
         * Must be balanced by a matching `endShaderMask()`.
         */
        void beginShaderMask(
            const Rect&   bounds,
            const Shader& shader,
            BlendMode     blend_mode = BlendMode::srcIn);

        /** @brief Closes a shader-mask scope opened with `beginShaderMask()`. */
        void endShaderMask();

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
            bool    is_layer          = false;
        };

        DrawList               commands_;
        Matrix4                current_transform_;
        Rect                   current_clip_;
        float                  current_opacity_ = 1.0f;
        std::vector<SaveEntry> save_stack_;
    };

} // namespace systems::leal::campello_widgets
