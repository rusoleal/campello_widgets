#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class BindGroup;
    class Sampler;
    class Texture;
    class Buffer;
    class CommandEncoder;
    class RenderPassEncoder;
}

namespace systems::leal::campello_widgets
{

// Real per-vertex data for the rect pipeline — see drawFilledQuad()/
// drawFilledVertices() below. Not `alignas(16)`: tightly-packed
// vertex-attribute element, not a uniform.
struct RectVertex {
    float x, y, w;
};

// Per-vertex data for the rect-AA pipeline (drawFillAA() below) — same
// layout as RectVertex plus a trailing per-vertex alpha, matching
// RectAAVertexIn's `posw_a` in widgets.metal.
struct RectAAVertex {
    float x, y, w, a;
};

// Per-vertex data for the vertices (drawVertices()) pipeline — same
// x,y,w convention as RectAAVertex (CPU-transformed clip-space position),
// plus a full per-vertex straight-alpha color instead of a single shared
// alpha, matching VerticesVertexIn's `posw`/`color` in widgets.metal. The
// color has already been paint-blended (Canvas::drawVertices() ->
// buildTriangleListVertices()) -- the fragment shader just outputs the
// GPU-interpolated color directly, no further blend math.
struct VerticesVertex {
    float x, y, w;
    float r, g, b, a;
};

// Per-vertex data for the liquid glass pipeline — see drawBackdropFilter()'s
// liquidGlass branch below. `uv` is the backdrop-texture sample coordinate
// (screen-space, like QuadVertex's); `lu`/`lv` is a plain 0..1 local
// parametrization of the widget's own rect, used by the shader to evaluate
// the rounded-rect SDF in a rotation-safe local space instead of screen
// space — see widgets.metal's Liquid Glass pipeline doc comment.
struct LiquidGlassVertex {
    float x, y, w, u, v, lu, lv;
};

// ---------------------------------------------------------------------------
// MetalDrawBackend
//
// IDrawBackend implementation for macOS/Metal.  Uses campello_gpu's public
// API to compile two render pipelines from the embedded .metallib:
//
//   rect_pipeline_  — solid-coloured filled quads  (rectVertex/rectFragment)
//   quad_pipeline_  — textured quads               (quadVertex/quadFragment)
//
// Text is pre-composited against bg_color_ at rasterisation time (using
// CoreText / CoreGraphics) since campello_gpu's ColorState does not expose
// GPU-side alpha blending.
//
// Call setViewport(w, h) once per frame before Renderer::renderFrame().
// ---------------------------------------------------------------------------
class MetalDrawBackend final : public IDrawBackend
{
public:
    MetalDrawBackend(
        std::shared_ptr<campello_gpu::Device> device,
        Color                                 bg_color,
        campello_gpu::PixelFormat             pixel_format);

    ~MetalDrawBackend() override = default;

    void drawRect(
        const DrawRectCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawCircle(
        const DrawCircleCmd&             cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawOval(
        const DrawOvalCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawRRect(
        const DrawRRectCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawLine(
        const DrawLineCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawArc(
        const DrawArcCmd&                cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawPath(
        const DrawPathCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawPoints(
        const DrawPointsCmd&             cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawVertices(
        const DrawVerticesCmd&           cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    std::shared_ptr<campello_gpu::Texture> renderPathFillWinding(
        const std::vector<Offset>& triangles,
        Path::FillType              fill_type,
        const Color&                color,
        uint32_t                    width,
        uint32_t                    height,
        campello_gpu::CommandEncoder& encoder) override;

    std::shared_ptr<campello_gpu::Texture> rasterizeText(
        const TextSpan& span, float dpr,
        uint32_t& out_width, uint32_t& out_height) override;

    std::shared_ptr<campello_gpu::BindGroup> drawTextTexture(
        std::shared_ptr<campello_gpu::Texture>   texture,
        std::shared_ptr<campello_gpu::BindGroup> cached_bind_group,
        uint32_t width, uint32_t height,
        const Offset&                     origin,
        const Matrix4&                    transform,
        const Rect&                       clip,
        campello_gpu::RenderPassEncoder&  encoder) override;

    void drawImage(
        const DrawImageCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawTintedImage(
        const DrawTintedImageCmd&        cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    Size measureText(const TextSpan& span) const override;
    Rect measureTextInkBounds(const TextSpan& span) const override;

    // ------------------------------------------------------------------
    // Offscreen / compositing (BackdropFilter + ShaderMask)
    // ------------------------------------------------------------------

    campello_gpu::PixelFormat offscreenPixelFormat() const noexcept override
    {
        return pixel_format_;
    }

    std::shared_ptr<campello_gpu::Texture> createOffscreenTexture(
        uint32_t width, uint32_t height) override;

    std::shared_ptr<campello_gpu::Texture> createDedicatedOffscreenTexture(
        uint32_t width, uint32_t height) override;

    std::shared_ptr<campello_gpu::RenderPassEncoder> beginOffscreenPass(
        std::shared_ptr<campello_gpu::Texture> tex,
        campello_gpu::CommandEncoder&          encoder,
        bool                                    preserve_content = false) override;

    std::shared_ptr<campello_gpu::Texture> blurTexture(
        std::shared_ptr<campello_gpu::Texture> source,
        float sigma_x, float sigma_y,
        campello_gpu::CommandEncoder& encoder) override;

    void drawBackdropFilter(
        const DrawBackdropFilterBeginCmd&      cmd,
        std::shared_ptr<campello_gpu::Texture> blurred_source,
        const Matrix4&                         transform,
        const Rect&                            clip,
        campello_gpu::RenderPassEncoder&       encoder) override;

    void drawShaderMaskComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const DrawShaderMaskBeginCmd&          cmd,
        const Matrix4&                         transform,
        const Rect&                            clip,
        campello_gpu::RenderPassEncoder&       encoder) override;

    void drawClipShapeComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const Rect&                            bounds,
        float                                  corner_radius,
        bool                                   is_oval,
        const Matrix4&                         transform,
        const Rect&                            clip,
        campello_gpu::RenderPassEncoder&       encoder) override;

    void saveLayerComposite(
        std::shared_ptr<campello_gpu::Texture> child_tex,
        const SaveLayerCmd&                    cmd,
        const Matrix4&                         transform,
        const Rect&                            clip,
        campello_gpu::RenderPassEncoder&       encoder) override;

    // ------------------------------------------------------------------

    void setViewport(float w, float h) noexcept override
    {
        setViewportSize(w, h);
        ++frame_counter_;
        rect_uniform_pool_.beginFrame();
        shape_uniform_pool_.beginFrame();
        line_uniform_pool_.beginFrame();
        quad_uniform_pool_.beginFrame();
        quad_vertex_pool_.beginFrame();
        icon_uniform_pool_.beginFrame();
        rect_vertex_pool_.beginFrame();
        vertices_vertex_pool_.beginFrame();
        vertices_index_pool_.beginFrame();
        vertices_uniform_pool_.beginFrame();
        clip_shape_uniform_pool_.beginFrame();
        shader_mask_uniform_pool_.beginFrame();
        liquid_glass_uniform_pool_.beginFrame();
        liquid_glass_vertex_pool_.beginFrame();
        offscreen_texture_pool_.beginFrame();
        offscreen_texture_pool_.evictStale(frame_counter_);
    }

    void setViewportSize(float w, float h) noexcept override
    {
        vp_w_ = w;
        vp_h_ = h;
        // Scissor cache is keyed by encoder-independent (x,y,w,h) values
        // only (see applyScissor()), so it must reset whenever we switch to
        // rendering into a different encoder — which is the only reason
        // callers other than setViewport() itself call this. Unlike
        // setViewport(), this deliberately does NOT touch frame_counter_ or
        // the uniform-buffer pools — see the doc comment on
        // IDrawBackend::setViewportSize() for why.
        last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
    }

    void setDevicePixelRatio(float dpr) noexcept override { dpr_ = dpr; }

    /** Returns true if all render pipelines were successfully compiled. */
    bool isValid() const noexcept { return rect_pipeline_ != nullptr; }

private:
    // Returns false if the clip is empty — callers must skip the draw in that case.
    bool applyScissor(const Rect& clip, campello_gpu::RenderPassEncoder& encoder);

    // One ambient-transform-projected corner: (x, y, w) is the result of
    // `transform * Vector4(local_x, local_y, 0, 1)`, kept as-is — never
    // collapsed to an axis-aligned bounding box the way this used to work
    // (see QuadVertexIn's doc comment in widgets.metal for why carrying a
    // real `w` through, instead of assuming 1, is what makes rotated/
    // perspective-projected quads render as genuinely tilted shapes with
    // correct texture mapping). (u, v) is this corner's own texture-space
    // sample coordinate — independent per corner, not derived from a
    // shared axis-aligned rect, because some callers (drawBackdropFilter)
    // sample a rotated/perspective region of a much larger source texture,
    // where each corner's UV depends on *that corner's own* projected
    // position, not a simple top-left/bottom-right UV span. Unused (left
    // zero) by callers that don't sample a texture, e.g. drawFilledQuad.
    struct ProjectedCorner { float x, y, w, u, v; };

    // Real per-vertex quad — see drawImage()/drawRect() for how corners are
    // computed (ambient-transform each corner independently, never collapse
    // to an axis-aligned bounding box). u/v in `c00`..`c11` are ignored.
    void drawFilledQuad(
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // Variable-length triangle batch for drawArc / drawPath. `count` must be
    // a multiple of 3; each vertex carries its own pre-projected (x,y,w).
    void drawFilledVertices(
        const std::vector<RectVertex>& verts,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // Same shape as drawFilledVertices(), but via rect_aa_pipeline_: each
    // vertex also carries its own alpha, multiplied into `color`'s alpha
    // and linearly interpolated by the rasterizer. Used for drawPath()'s
    // fill antialiasing skirt — see src/gpu/path_fill_aa.hpp and this
    // header's RectAAVertex doc comment.
    void drawFillAA(
        const std::vector<RectAAVertex>& verts,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawShape(
        float x, float y, float w, float h,
        float corner_r, float stroke_w, float kind,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // ------------------------------------------------------------------
    // Stroke primitives — GPU-side cap/join expansion (see
    // src/gpu/stroke_geometry.hpp's doc comment for the overall design:
    // buildStrokeGeometry() does O(1)-per-element CPU vector math to decide
    // *what* to draw; these methods do the actual antialiased GPU rendering).
    // Points are in local (pre-transform) space, matching drawLine()/
    // drawRect()/drawPath()'s existing convention.
    // ------------------------------------------------------------------

    // One antialiased, butt-ended segment body via line_pipeline_.
    void drawStrokeSegmentBody(
        const Offset& p0, const Offset& p1, float half_width,
        const Color& color, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // One antialiased round cap/join, as a filled circle via drawShape().
    void drawStrokeRoundPrimitive(
        const Offset& center, float half_width,
        const Color& color, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // Strokes a polyline as individual GPU draw calls (segment bodies +
    // round caps/joins via drawShape() + bevel/miter wedges via
    // drawFilledVertices()). For drawLine()/drawRect() stroke, where point
    // count is always small — same "one shape = a few draw calls" cost
    // class as every other shape primitive in this backend.
    void strokePolyline(
        const std::vector<Offset>& points, bool closed,
        const Paint& paint, const Rect& clip, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // Same decomposition as strokePolyline(), but appends flat (non-AA)
    // triangles to `verts` instead of issuing per-primitive draw calls —
    // preserves drawPath()'s existing "one draw call regardless of segment
    // count" property for potentially long flattened curves. Round caps/
    // joins are approximated as a small triangle fan (no SDF available in
    // this batched context).
    void appendStrokePolylineBatched(
        const std::vector<Offset>& points, bool closed,
        const Paint& paint, const Matrix4& transform,
        std::vector<RectVertex>& verts);

    // `cached_bind_group`, if non-null, is reused as-is instead of building a
    // fresh BindGroup for `texture` (see Renderer::text_texture_cache_'s doc
    // comment — the bind group is cached alongside the texture there and
    // stays valid for as long as that cache entry does).
    //
    // c00/c10/c01/c11 are this quad's four corners (top-left, top-right,
    // bottom-left, bottom-right) — see ProjectedCorner's doc comment above.
    //
    // Returns the BindGroup actually used — either `cached_bind_group`
    // passed straight through, or a freshly built one (nullptr if drawing
    // was aborted, e.g. missing pipeline). Callers that want to skip
    // rebuilding it next time should keep it and pass it back in.
    // `sampler`: nullptr (default) uses quad_sampler_ (linear), matching
    // every existing call site's behavior unchanged. drawImage() passes
    // nearest_sampler_ explicitly when the caller requests
    // FilterQuality::none. Only takes effect when `cached_bind_group` is
    // null (a supplied cached_bind_group already has its own sampler
    // baked in from whenever it was first built) — true for all current
    // callers of the `sampler` parameter, since none of them pass a cache.
    std::shared_ptr<campello_gpu::BindGroup> drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder,
        std::shared_ptr<campello_gpu::BindGroup> cached_bind_group = nullptr,
        std::shared_ptr<campello_gpu::Sampler>   sampler = nullptr);

    // Mirrors drawTexturedQuad() exactly, but binds icon_pipeline_ (which
    // samples only the source texture's alpha channel and recolors with
    // `tint` — see iconFragment in widgets.metal) instead of quad_pipeline_.
    // Kept as its own method rather than adding a tint parameter to
    // drawTexturedQuad() so every existing call site (text glyphs, plain
    // images, backdrop-filter compositing) is untouched.
    // `sampler`: see drawTexturedQuad()'s doc comment.
    void drawTintedTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        const ProjectedCorner& c00, const ProjectedCorner& c10,
        const ProjectedCorner& c01, const ProjectedCorner& c11,
        const Color& tint,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder,
        std::shared_ptr<campello_gpu::Sampler>   sampler = nullptr);

    // Utility: build and run a single-pass blur render into `dst`.
    void runBlurPass(
        std::shared_ptr<campello_gpu::Texture> src,
        std::shared_ptr<campello_gpu::Texture> dst,
        float sigma,
        bool  horizontal,
        campello_gpu::CommandEncoder& encoder);

    // Build a 256×1 RGBA LUT texture from gradient colors/stops.
    std::shared_ptr<campello_gpu::Texture> buildGradientLUT(
        const std::vector<Color>& colors,
        const std::vector<float>& stops);

    // ------------------------------------------------------------------
    // Uniform buffer pool
    //
    // drawFilledRect/drawShape/drawLine/drawTexturedQuad each called
    // Device::createBuffer() — a real GPU allocation — on every single draw,
    // purely to hold a few floats of per-draw uniform data. This pool
    // amortizes the allocation: each draw still gets fresh contents via
    // Buffer::upload(), but the underlying GPU buffer objects are reused
    // round-robin across a small ring of frame "generations" instead of
    // being allocated from scratch every time. kGenerations=4 only needs to
    // exceed how many frames' worth of command buffers might still be
    // in-flight on the GPU when a ring slot comes back around for CPU
    // reuse — it doesn't need to track the real in-flight depth precisely.
    // ------------------------------------------------------------------

    class UniformBufferPool
    {
    public:
        std::shared_ptr<campello_gpu::Buffer> acquire(
            campello_gpu::Device& device, uint64_t size, const void* data);

        // Advances to the next ring slot; called once per frame.
        void beginFrame() noexcept;

    private:
        static constexpr size_t kGenerations = 4;
        std::array<std::vector<std::shared_ptr<campello_gpu::Buffer>>, kGenerations> generations_;
        std::array<size_t, kGenerations>                                              next_index_{};
        size_t                                                                        current_generation_ = 0;
    };

    // ------------------------------------------------------------------
    // Offscreen texture pool
    //
    // applyClipShape()/applyShaderMask() (Renderer, via createOffscreenTexture())
    // used to call Device::createTexture() — a real GPU allocation — on
    // every single ClipRRect/ClipOval/ShaderMask composite, every frame.
    // For a continuously-animating widget wrapped in one of these (the
    // motivating case: an animated Transform demo with 4 ClipRRect-wrapped
    // images), that's 4 fresh GPU texture allocations every single frame,
    // forever, purely from the animation — see TODO.md's "Paint/Compositing
    // Architecture" Stage 0d writeup for the investigation that found this.
    // Same amortization idea as UniformBufferPool above, adapted for
    // textures: keyed by (width, height) since offscreen bounds vary per
    // widget (unlike uniform buffers, which have one fixed struct size per
    // pool instance), with the same kGenerations-deep ring so a slot isn't
    // reused for a fresh write until enough frames have passed that the GPU
    // is done reading the texture from its last use.
    //
    // Unlike text/uniform-buffer reuse, no upload() step is needed on a
    // cache hit — beginOffscreenPass()'s loadOp=clear already re-clears the
    // texture before each use.
    //
    // A distinct (width, height) key stops being requested whenever a
    // widget resizes or leaves the tree, so unused size buckets are evicted
    // after kMaxAgeFrames unused, mirroring Renderer::evictStaleGpuCaches()'s
    // age-based sweep (which now also covers text texture caching, moved
    // there from this backend), to avoid growing unboundedly across window
    // resizes.
    // ------------------------------------------------------------------

    class OffscreenTexturePool
    {
    public:
        std::shared_ptr<campello_gpu::Texture> acquire(
            campello_gpu::Device& device, uint32_t width, uint32_t height,
            campello_gpu::PixelFormat format, campello_gpu::TextureUsage usage,
            uint64_t current_frame);

        // Advances to the next ring slot; called once per frame.
        void beginFrame() noexcept;

        // Drops any size bucket not acquired in the last kMaxAgeFrames frames.
        void evictStale(uint64_t current_frame);

    private:
        struct SizeKey
        {
            uint32_t width  = 0;
            uint32_t height = 0;
            bool operator==(const SizeKey&) const noexcept = default;
        };
        struct SizeKeyHash
        {
            size_t operator()(const SizeKey& k) const noexcept
            {
                return (static_cast<size_t>(k.width) << 32) ^ static_cast<size_t>(k.height);
            }
        };

        static constexpr size_t   kGenerations  = 4;
        static constexpr uint64_t kMaxAgeFrames = 120;

        std::array<std::unordered_map<SizeKey, std::vector<std::shared_ptr<campello_gpu::Texture>>, SizeKeyHash>, kGenerations> generations_;
        std::array<std::unordered_map<SizeKey, size_t, SizeKeyHash>, kGenerations>                                              next_index_;
        std::unordered_map<SizeKey, uint64_t, SizeKeyHash>                                                                      last_used_frame_;
        size_t current_generation_ = 0;
    };

    uint64_t frame_counter_ = 0;

    UniformBufferPool rect_uniform_pool_;
    UniformBufferPool shape_uniform_pool_;
    UniformBufferPool line_uniform_pool_;
    UniformBufferPool quad_uniform_pool_;

    // Icon pipeline's uniforms (viewport/opacity/tint — larger than
    // QuadUniforms, so it needs its own pool rather than sharing
    // quad_uniform_pool_'s ring). Vertex geometry is identical in shape to
    // the quad pipeline's (QuadVertex: x,y,w,u,v), so icon draws reuse
    // quad_vertex_pool_ directly instead of a dedicated pool.
    UniformBufferPool icon_uniform_pool_;

    // Separate pool for the quad pipeline's real per-vertex data (QuadVertex
    // arrays — see drawTexturedQuad()). Must be its own pool instance, not
    // shared with quad_uniform_pool_: UniformBufferPool's ring reuses a
    // buffer slot via Buffer::upload(0, size, data) assuming every acquire()
    // against one pool instance uses the same size — mixing the small fixed
    // QuadUniforms struct with variable-length vertex arrays in one pool
    // would upload() past a buffer sized for the other.
    UniformBufferPool quad_vertex_pool_;

    // Same reasoning as quad_vertex_pool_ above, for the rect pipeline's
    // per-vertex data (RectVertex arrays — see drawFilledQuad()).
    UniformBufferPool rect_vertex_pool_;

    // drawVertices()'s own vertex (VerticesVertex arrays) and index
    // (uint32_t arrays) buffers — separate pools since the two hold
    // different element types/sizes, same reasoning as
    // quad_vertex_pool_/rect_vertex_pool_ above.
    UniformBufferPool vertices_vertex_pool_;
    UniformBufferPool vertices_index_pool_;
    UniformBufferPool vertices_uniform_pool_;

    // ClipShapeUniforms pool — drawClipShapeComposite() previously called
    // device_->createBuffer() directly, unpooled, on every single
    // ClipRRect/ClipOval composite, every frame. Pooled now for the same
    // reason as everything else in this class (see OffscreenTexturePool's
    // doc comment above for the motivating case: continuously-animating
    // ClipRRect content triggers this on every frame).
    UniformBufferPool clip_shape_uniform_pool_;

    // Same as clip_shape_uniform_pool_ above, for drawShaderMaskComposite().
    UniformBufferPool shader_mask_uniform_pool_;

    // LiquidGlassUniforms pool — small, fixed-size, same reasoning as
    // clip_shape_uniform_pool_/shader_mask_uniform_pool_ above.
    UniformBufferPool liquid_glass_uniform_pool_;

    // Separate pool for the liquid glass pipeline's per-vertex data
    // (LiquidGlassVertex arrays), for the same reason quad_vertex_pool_ is
    // separate from quad_uniform_pool_ — see that field's doc comment.
    UniformBufferPool liquid_glass_vertex_pool_;

    OffscreenTexturePool offscreen_texture_pool_;

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  rect_aa_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  vertices_pipeline_;

    // Path::fillType() stencil-then-cover pipelines -- see
    // Renderer::applyPathFillWinding()'s doc comment. All three reuse
    // rectVertex/rectFragment (widgets.metal) and RectVertex/RectUniforms
    // verbatim -- only the ColorState/depthStencil configuration differs
    // from rect_pipeline_, so no new shader code at all. The two write
    // variants only differ in their stencil passOp (incrementWrap/
    // decrementWrap per front/back face for `winding`, `invert` on both
    // faces for `evenOdd` -- see renderPathFillWinding()'s doc comment for
    // why one shared cover pipeline works for both fill types).
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_write_winding_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_write_evenodd_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_cover_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  line_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  quad_bgl_;
    std::shared_ptr<campello_gpu::Sampler>          quad_sampler_;

    // Nearest-neighbor counterpart to quad_sampler_ (linear), selected by
    // drawImage()/drawTintedImage() only, per-draw, when the caller passes
    // FilterQuality::none. Every other texture-sampling draw always uses
    // quad_sampler_.
    std::shared_ptr<campello_gpu::Sampler>          nearest_sampler_;

    // Icon pipeline (tinted template images — see iconVertex/iconFragment
    // in widgets.metal). Reuses quad_bgl_/quad_sampler_ — same texture@0 +
    // sampler@1 binding shape as the quad pipeline, just a different
    // fragment shader and its own (larger) uniform struct.
    std::shared_ptr<campello_gpu::RenderPipeline>  icon_pipeline_;

    // Blur pipeline (reuses quad_bgl_ for texture+sampler binding).
    std::shared_ptr<campello_gpu::RenderPipeline>  blur_pipeline_;

    // Liquid Glass pipeline (also reuses quad_bgl_/quad_sampler_ — same
    // texture@0 + sampler@1 binding shape, just a different fragment
    // shader and its own vertex layout with the extra local_uv attribute).
    std::shared_ptr<campello_gpu::RenderPipeline>  liquid_glass_pipeline_;

    // ShaderMask pipeline (child tex + LUT tex + sampler).
    std::shared_ptr<campello_gpu::RenderPipeline>  shader_mask_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  shader_mask_bgl_;

    // ClipRRect/ClipOval pipeline (reuses quad_bgl_: child tex@0, sampler@1).
    std::shared_ptr<campello_gpu::RenderPipeline>  clip_shape_pipeline_;

    // Persistent blur scratch textures (resized on demand).
    std::shared_ptr<campello_gpu::Texture>          blur_h_tex_;
    std::shared_ptr<campello_gpu::Texture>          blur_v_tex_;
    uint32_t                                        blur_tex_w_ = 0;
    uint32_t                                        blur_tex_h_ = 0;

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;
    float dpr_  = 1.0f;

    // Scissor-state cache — avoids redundant setScissorRect calls that trigger
    // Metal API Validation asserts.
    float last_scissor_x_ = -1.0f;
    float last_scissor_y_ = -1.0f;
    float last_scissor_w_ = -1.0f;
    float last_scissor_h_ = -1.0f;
};

} // namespace systems::leal::campello_widgets
