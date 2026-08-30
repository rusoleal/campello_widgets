#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <vector_math/matrix4.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/color.hpp>

// campello_gpu forward declaration — avoids pulling the full GPU headers
// into every widget translation unit.
namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    using Matrix4 = systems::leal::vector_math::Matrix4<float>;

    // ------------------------------------------------------------------
    // New draw command types for Flutter Canvas API compatibility
    // ------------------------------------------------------------------

    /** @brief Draw a circle with center and radius. */
    struct DrawCircleCmd
    {
        Offset center;
        float  radius;
        Paint  paint;
    };

    /** @brief Draw an oval filling the given rectangle. */
    struct DrawOvalCmd
    {
        Rect  rect;
        Paint paint;
    };

    /** @brief Draw an arc. */
    struct DrawArcCmd
    {
        Rect  rect;           // Bounding rect (oval)
        float start_angle;    // Radians
        float sweep_angle;    // Radians
        bool  use_center;     // If true, draws a pie slice; if false, just the arc
        Paint paint;
    };

    /** @brief Draw a line between two points. */
    struct DrawLineCmd
    {
        Offset p1;
        Offset p2;
        Paint  paint;
    };

    /** @brief Draw a rounded rectangle. */
    struct DrawRRectCmd
    {
        RRect rrect;
        Paint paint;
    };

    /** @brief Draw a path. */
    struct DrawPathCmd
    {
        Path  path;
        Paint paint;
    };

    /** @brief Draw points (points, lines, or polygon). */
    enum class PointMode
    {
        points,     // Individual points
        lines,      // Lines between consecutive pairs
        polygon,    // Lines connecting all points in a loop
    };

    struct DrawPointsCmd
    {
        PointMode           mode;
        std::vector<Offset> points;
        Paint               paint;
    };

    /**
     * @brief One vertex of a `DrawVerticesCmd` -- position plus its final,
     * already paint-blended straight-alpha color. See
     * `buildTriangleListVertices()` (src/ui/vertices_geometry.hpp) for how
     * the blend is computed once per vertex on the CPU rather than
     * per-fragment on the GPU.
     */
    struct VertexColorPoint
    {
        Offset pos;
        Color  color;
    };

    /**
     * @brief Draw an arbitrary triangle mesh (`Canvas::drawVertices()`) --
     * always a flat triangle list (`indices.size() % 3 == 0`), already
     * normalised from whatever `VertexMode`/index form the caller used. See
     * `Canvas::drawVertices()`'s doc comment for scope (per-vertex color
     * only -- no texture coordinates, no mesh-edge antialiasing).
     *
     * `blend_mode` is `Paint::blend_mode` (the GPU framebuffer-compositing
     * mode every other draw command's `Paint` carries) -- unrelated to
     * `Canvas::drawVertices()`'s own `blend_mode` *parameter*, which
     * combines `Paint::color` with each vertex's own color and is already
     * fully consumed into `vertices`' baked-in colors by the time this
     * command is recorded.
     */
    struct DrawVerticesCmd
    {
        std::vector<VertexColorPoint> vertices;
        std::vector<uint32_t>         indices;
        BlendMode                     blend_mode = BlendMode::srcOver;
    };

    /** @brief Draw a shadow for a path (material elevation). */
    struct DrawShadowCmd
    {
        Path   path;
        Color  color;
        float  elevation;
        bool   transparent_occluder;
    };

    /** @brief Save with a layer (for compositing). */
    struct SaveLayerCmd
    {
        Rect  bounds;  // Nullable - if empty, uses current clip
        Paint paint;   // For color filter, blend mode
    };

    /** @brief Ends a compositing layer opened with SaveLayerCmd. */
    struct SaveLayerEndCmd {};

    // ------------------------------------------------------------------
    // BackdropFilter commands
    // ------------------------------------------------------------------

    /**
     * @brief Marks the start of a BackdropFilter region.
     *
     * The Renderer captures the scene rendered *before* this point into an
     * offscreen texture, blurs it using `filter`, and draws it as the
     * background of the bounds.  Commands between this and
     * DrawBackdropFilterEndCmd are the widget's children, which render on
     * top of the blurred backdrop.
     *
     * During the backdrop-capture pass the child commands are skipped so
     * that the source texture only contains the scene behind the widget.
     */
    struct DrawBackdropFilterBeginCmd
    {
        Rect        bounds;
        ImageFilter filter;
    };

    /** @brief Marks the end of a BackdropFilter child scope. */
    struct DrawBackdropFilterEndCmd {};

    // ------------------------------------------------------------------
    // ShaderMask commands
    // ------------------------------------------------------------------

    /**
     * @brief Marks the start of a ShaderMask region.
     *
     * Commands between this and DrawShaderMaskEndCmd are the widget's
     * children.  The Renderer renders those children to an offscreen
     * texture, evaluates `shader` as a mask, composites the two using
     * `blend_mode`, and draws the result into the main target.
     */
    struct DrawShaderMaskBeginCmd
    {
        Rect      bounds;
        Shader    shader;
        BlendMode blend_mode = BlendMode::srcIn;
    };

    /** @brief Marks the end of a ShaderMask child scope. */
    struct DrawShaderMaskEndCmd {};

    /** @brief Clip using a path. */
    struct PushClipPathCmd
    {
        Path path;
    };

    /** @brief Clip using a rounded rectangle. */
    struct PushClipRRectCmd
    {
        RRect rrect;
    };

    /** @brief Clip using an oval inscribed in a bounding rectangle. */
    struct PushClipOvalCmd
    {
        Rect rect;
    };

    // ------------------------------------------------------------------
    // Individual command types
    // ------------------------------------------------------------------

    /** @brief Draw a filled or stroked axis-aligned rectangle. */
    struct DrawRectCmd
    {
        Rect  rect;
        Paint paint;
    };

    /**
     * @brief Draw a text span at the given origin.
     *
     * The origin is the top-left corner of the text's bounding box
     * in the current transform's coordinate space.
     */
    struct DrawTextCmd
    {
        TextSpan span;
        Offset   origin;
    };

    /**
     * @brief Draw a GPU texture into a destination rectangle.
     *
     * @param texture  The source texture (must be valid for the draw).
     * @param src_rect Normalized source rectangle [0,1]×[0,1] within the texture.
     *                 Pass `Rect::fromLTWH(0,0,1,1)` for the whole texture.
     * @param dst_rect Destination rectangle in local coordinates.
     */
    struct DrawImageCmd
    {
        std::shared_ptr<campello_gpu::Texture> texture;
        Rect  src_rect;
        Rect  dst_rect;
        float opacity = 1.0f;
        FilterQuality filter_quality = FilterQuality::high;
    };

    /**
     * @brief Draw a "template" texture recolored to an arbitrary tint.
     *
     * The source texture's own RGB is ignored entirely; only its alpha
     * channel is sampled, used as a stencil for `tint` — the same
     * mechanism as iOS's `UIImage.withRenderingMode(.alwaysTemplate)` and
     * Android's icon tinting. Lets a single monochrome icon asset serve
     * any theme color instead of needing a pre-baked PNG per tint. Used
     * by the `Icon` widget.
     *
     * @param texture  The source (template) texture.
     * @param src_rect Normalized source rectangle [0,1]×[0,1] within the texture.
     * @param dst_rect Destination rectangle in local coordinates.
     * @param tint     Straight-alpha recolor applied via the texture's alpha.
     */
    struct DrawTintedImageCmd
    {
        std::shared_ptr<campello_gpu::Texture> texture;
        Rect  src_rect;
        Rect  dst_rect;
        Color tint;
        float opacity = 1.0f;
        FilterQuality filter_quality = FilterQuality::high;
    };

    /** @brief Push a new clip rectangle onto the clip stack (intersects with current). */
    struct PushClipRectCmd
    {
        Rect rect;
    };

    /** @brief Pop the top clip rectangle from the stack. */
    struct PopClipRectCmd {};

    /** @brief Push a transform matrix onto the transform stack (post-multiplied). */
    struct PushTransformCmd
    {
        Matrix4 transform;
    };

    /** @brief Pop the top transform from the stack. */
    struct PopTransformCmd {};

    // ------------------------------------------------------------------
    // Cache-replay markers
    // ------------------------------------------------------------------

    /**
     * @brief Marks the start of a replayed (cache-hit) OffsetLayer region.
     *
     * Emitted by `OffsetLayer::maybeReplay()`'s identity-replay path,
     * wrapped around a byte-for-byte copy of a previously-recorded
     * `DrawList` slice. Every command between this and the matching
     * `CacheReplayEndCmd` is guaranteed identical to what was recorded the
     * last time this exact region was captured — nothing in it changed,
     * or `OffsetLayer` would have forced a full re-record instead of
     * replaying. `Renderer::flushDrawList()` uses this guarantee to reuse
     * a previous frame's GPU composite for any `ClipRRect`/`ClipOval`/
     * `ShaderMask` bracket found inside, instead of recapturing it.
     *
     * `region_id` is `this` from the emitting `OffsetLayer`, used purely
     * as an opaque, never-dereferenced map key. `incarnation_id` is that
     * same `OffsetLayer`'s own construction-order counter — see
     * `Renderer::clip_shape_gpu_cache_`'s doc comment for why the raw
     * address alone isn't a safe cache key once the owning RenderObject
     * can unmount and a later, unrelated OffsetLayer can be allocated at
     * the same address (e.g. a virtualized `ListView` recycling scrolled-
     * out item RenderObjects).
     */
    struct CacheReplayBeginCmd { const void* region_id; uint64_t incarnation_id = 0; };

    /** @brief Marks the end of a `CacheReplayBeginCmd` region. */
    struct CacheReplayEndCmd {};

    // ------------------------------------------------------------------
    // DrawSurface incremental-update commands
    // ------------------------------------------------------------------

    /**
     * @brief Marks the start of an incremental update to a persistent
     * drawing surface's texture (see `RenderDrawSurface`).
     *
     * Unlike ShaderMask/ClipRRect's offscreen brackets — which capture
     * their children fresh into a throwaway texture every single frame —
     * commands between this and `DrawSurfaceUpdateEndCmd` are rendered
     * into `target` with the texture's *existing* content preserved
     * (`clear_first == false`) rather than cleared, so only the newly
     * added strokes since the last update need to be submitted. `target`
     * is then displayed like any other texture via a normal `DrawImageCmd`
     * emitted right after this bracket — the Renderer never composites it
     * itself.
     *
     * `blit_source`, when set, is a previous (differently-sized) surface
     * texture whose content should be copied into `target`'s top-left
     * corner before any of this bracket's draw commands run — used when
     * a `RenderDrawSurface` is resized: a GPU texture can't be resized in
     * place, so a fresh one is allocated, but the prior strokes shouldn't
     * simply vanish (matches how a real drawing app crops/extends a
     * canvas on resize rather than clearing it). Mutually exclusive with
     * `clear_first` in practice — a caller that's blitting prior content
     * in never also wants it cleared.
     */
    struct DrawSurfaceUpdateBeginCmd
    {
        std::shared_ptr<campello_gpu::Texture> target;
        bool                                    clear_first = false;
        std::shared_ptr<campello_gpu::Texture> blit_source;
    };

    /** @brief Marks the end of a `DrawSurfaceUpdateBeginCmd` region. */
    struct DrawSurfaceUpdateEndCmd {};

    // ------------------------------------------------------------------
    // Variant
    // ------------------------------------------------------------------

    /**
     * @brief Tagged union of all draw operations emitted by PaintContext.
     *
     * The Renderer collects these from each PaintContext after a paint pass
     * and dispatches them to the registered IDrawBackend for GPU execution.
     */
    using DrawCommand = std::variant<
        // Basic shapes
        DrawRectCmd,
        DrawCircleCmd,
        DrawOvalCmd,
        DrawArcCmd,
        DrawLineCmd,
        DrawRRectCmd,
        DrawPointsCmd,

        // Complex shapes
        DrawPathCmd,
        DrawShadowCmd,
        DrawVerticesCmd,

        // Text and images
        DrawTextCmd,
        DrawImageCmd,
        DrawTintedImageCmd,

        // Clipping
        PushClipRectCmd,
        PushClipRRectCmd,
        PushClipOvalCmd,
        PushClipPathCmd,
        PopClipRectCmd,

        // Transforms and state
        PushTransformCmd,
        PopTransformCmd,
        SaveLayerCmd,
        SaveLayerEndCmd,

        // BackdropFilter (begin/end pair wrapping child commands)
        DrawBackdropFilterBeginCmd,
        DrawBackdropFilterEndCmd,

        // ShaderMask (begin/end pair wrapping child commands)
        DrawShaderMaskBeginCmd,
        DrawShaderMaskEndCmd,

        // Cache-replay markers (begin/end pair wrapping a verbatim replay)
        CacheReplayBeginCmd,
        CacheReplayEndCmd,

        // DrawSurface incremental update (begin/end pair wrapping new strokes)
        DrawSurfaceUpdateBeginCmd,
        DrawSurfaceUpdateEndCmd
    >;

    using DrawList = std::vector<DrawCommand>;

} // namespace systems::leal::campello_widgets
