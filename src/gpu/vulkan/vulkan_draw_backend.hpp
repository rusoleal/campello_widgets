#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <array>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class BindGroup;
    class PipelineLayout;
    class Sampler;
    class Texture;
    class Buffer;
}

namespace systems::leal::campello_widgets
{

class ITextRasterizer;

// ---------------------------------------------------------------------------
// VulkanDrawBackend
//
// IDrawBackend implementation for Vulkan — shared by Android and Linux (the
// two platforms that target campello_gpu's Vulkan backend). Uses
// campello_gpu's public API with pre-compiled SPIR-V shaders. Platform-native
// text rasterization (JNI Canvas/Paint on Android, FreeType+HarfBuzz on
// Linux) is injected via ITextRasterizer / createPlatformTextRasterizer(),
// the one genuinely OS-specific seam.
//
// Supported:  drawRect, drawRRect, drawImage, drawCircle, drawOval, drawLine,
//             drawPoints, drawArc, text (rasterizeText/drawTextTexture),
//             createOffscreenTexture, beginOffscreenPass, blurTexture,
//             drawBackdropFilter, drawClipShapeComposite, drawShaderMaskComposite
//
// Call setViewport(w, h) once per frame before Renderer::renderFrame().
// ---------------------------------------------------------------------------
class VulkanDrawBackend final : public IDrawBackend
{
public:
    VulkanDrawBackend(
        std::shared_ptr<campello_gpu::Device> device,
        Color                                 bg_color,
        campello_gpu::PixelFormat             pixel_format);

    ~VulkanDrawBackend() override;

    void drawRect(
        const DrawRectCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawRRect(
        const DrawRRectCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

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

    void drawLine(
        const DrawLineCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawPoints(
        const DrawPointsCmd&             cmd,
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

    Size measureText(const TextSpan& span) const override;

    void setViewport(float w, float h) noexcept override
    {
        // campello_gpu's Vulkan Device::submit() no longer blocks until the
        // GPU is done (kFramesInFlight-deep pipelining instead — see its
        // doc comment in campello_gpu/src/vulkan/device.cpp), so the
        // immediately-preceding frame's GPU work may still be executing
        // when this runs. This call itself runs *before* this frame's own
        // Device::createCommandEncoder()/beginFrameRing() wait — the most
        // recently confirmed-done generation at this point is only as
        // fresh as the *previous* frame's beginFrameRing() call, which
        // waited for kFramesInFlight=2 generations back. So it takes two
        // deferred generations here (prev_/prev2_), not one, before it's
        // provably safe to actually free anything:
        //   frame G's setViewport() only just-confirmed generation G-3 done
        //   (via generation G-1's own beginFrameRing() wait, which already
        //   ran before this call) — see the reasoning traced out in this
        //   fix's PR description if this ever needs re-deriving.
        prev2_frame_buffers_.clear();
        prev2_frame_textures_.clear();
        prev2_frame_views_.clear();
        std::swap(prev2_frame_buffers_,  prev_frame_buffers_);
        std::swap(prev2_frame_textures_, prev_frame_textures_);
        std::swap(prev2_frame_views_,    prev_frame_views_);
        std::swap(prev_frame_buffers_,   frame_buffers_);
        std::swap(prev_frame_textures_,  frame_textures_);
        std::swap(prev_frame_views_,     frame_views_);
        colored_quad_vertex_pool_.beginFrame();
        quad_vertex_pool_.beginFrame();
        vertices_vertex_pool_.beginFrame();
        vertices_index_pool_.beginFrame();
        setViewportSize(w, h);
    }

    void setViewportSize(float w, float h) noexcept override
    {
        vp_w_ = w; vp_h_ = h;
        // Each beginRenderPass() resets the GPU scissor; invalidate the cache
        // so the first applyScissor() in any new pass always emits vkCmdSetScissor.
        last_scissor_x_ = -1.0f;
        last_scissor_y_ = -1.0f;
        last_scissor_w_ = -1.0f;
        last_scissor_h_ = -1.0f;
    }

    void onBeginFlush() noexcept override
    {
        // beginRenderPass() resets the GPU scissor to the full render area.
        // Invalidate our cache so the first applyScissor() call always emits
        // vkCmdSetScissor rather than incorrectly reusing the previous value.
        last_scissor_x_ = -1.0f;
        last_scissor_y_ = -1.0f;
        last_scissor_w_ = -1.0f;
        last_scissor_h_ = -1.0f;
    }

    campello_gpu::PixelFormat offscreenPixelFormat() const noexcept override
    {
        return pixel_format_;
    }

    void setDevicePixelRatio(float dpr) noexcept override { dpr_ = dpr; }

    std::shared_ptr<campello_gpu::Texture> createOffscreenTexture(
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
        const DrawBackdropFilterBeginCmd&             cmd,
        std::shared_ptr<campello_gpu::Texture>        blurred_source,
        const Matrix4&                                transform,
        const Rect&                                   clip,
        campello_gpu::RenderPassEncoder&              encoder) override;

    void drawClipShapeComposite(
        std::shared_ptr<campello_gpu::Texture>        child_tex,
        const Rect&                                   bounds,
        float                                          corner_radius,
        bool                                           is_oval,
        const Matrix4&                                transform,
        const Rect&                                   clip,
        campello_gpu::RenderPassEncoder&              encoder) override;

    void drawShaderMaskComposite(
        std::shared_ptr<campello_gpu::Texture>        child_tex,
        const DrawShaderMaskBeginCmd&                 cmd,
        const Matrix4&                                transform,
        const Rect&                                   clip,
        campello_gpu::RenderPassEncoder&              encoder) override;

    void saveLayerComposite(
        std::shared_ptr<campello_gpu::Texture>        child_tex,
        const SaveLayerCmd&                           cmd,
        const Matrix4&                                transform,
        const Rect&                                   clip,
        campello_gpu::RenderPassEncoder&              encoder) override;

    // Per-vertex projected position + UV — mirrors Metal's ProjectedCorner.
    struct QuadCorner { float x, y, w, u, v; };

    // Per-vertex projected position (no UV) for colored_quad_pipeline_ —
    // was previously redefined identically as a local struct inside
    // drawRect()/drawArc()/drawPath(); hoisted here so
    // appendStrokePolylineBatched() can share it with drawPath()'s own
    // vertex vector.
    struct ColoredQuadVertex { float x, y, w; };

    // Same layout as ColoredQuadVertex plus a per-vertex alpha, for
    // rect_aa_pipeline_ — see src/gpu/path_fill_aa.hpp's doc comment and
    // drawPath()'s fill branch below.
    struct RectAAVertex { float x, y, w, a; };

    // Per-vertex data for vertices_pipeline_ (drawVertices()) — same x,y,w
    // convention as RectAAVertex (CPU-transformed clip-space position),
    // plus a full per-vertex straight-alpha color instead of a single
    // shared alpha. Already paint-blended by the time it reaches here —
    // see Canvas::drawVertices()'s doc comment.
    struct VerticesVertex { float x, y, w, r, g, b, a; };

    // ------------------------------------------------------------------
    // Uniform buffer pool — mirrors MetalDrawBackend::UniformBufferPool
    // exactly (see its doc comment). drawFilledQuad/drawTexturedQuad/
    // drawClipShapeComposite each called Device::createBuffer() — a real
    // VkBuffer + vkAllocateMemory, not just a memcpy — on every single
    // draw, purely to hold six vertices' worth of per-draw geometry.
    // Found costing ~8-13% of per-draw-call time on Android/Adreno (real
    // device measurement, Galaxy Tab S7 FE): this pool amortizes the
    // allocation the same way Metal's already does — each draw still gets
    // fresh contents via Buffer::upload(), but the underlying GPU buffer
    // objects are reused round-robin across a small ring of frame
    // "generations" instead of being allocated from scratch every time.
    // kGenerations=4 gives comfortable margin over the 2-3 generations
    // this backend's own frame_buffers_/prev_frame_buffers_/
    // prev2_frame_buffers_ retention scheme already proves necessary for
    // safety (see setViewport()'s doc comment) — a slot is only reused
    // once every kGenerations beginFrame() calls, by which point the GPU
    // is long done with whatever it last read from that slot. Pooled
    // buffers are NOT also pushed into frame_buffers_ — the pool itself
    // owns them permanently, so there's nothing to retain-then-free.
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

    // Same ring-pool shape as UniformBufferPool, but the underlying VkBuffer
    // is created with BufferUsage::index instead of ::vertex -- Vulkan
    // requires the index-buffer usage bit explicitly (unlike Metal/D3D,
    // which don't distinguish buffer "kinds" at creation time), so
    // UniformBufferPool's vertex-only buffers can't be bound via
    // setIndexBuffer(). Used by drawVertices() for its triangle-list index
    // data.
    class IndexBufferPool
    {
    public:
        std::shared_ptr<campello_gpu::Buffer> acquire(
            campello_gpu::Device& device, uint64_t size, const void* data);
        void beginFrame() noexcept;

    private:
        static constexpr size_t kGenerations = 4;
        std::array<std::vector<std::shared_ptr<campello_gpu::Buffer>>, kGenerations> generations_;
        std::array<size_t, kGenerations>                                              next_index_{};
        size_t                                                                        current_generation_ = 0;
    };

private:
    void applyScissor(
        const Rect& clip,
        campello_gpu::RenderPassEncoder& encoder);

    std::shared_ptr<campello_gpu::RenderPipeline> pipelineForBlendMode(
        BlendMode mode,
        const std::shared_ptr<campello_gpu::RenderPipeline>& base,
        const std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>>& variants) const;

    // ------------------------------------------------------------------
    // Stroke primitives — see src/gpu/stroke_geometry.hpp's doc comment for
    // the overall design: buildStrokeGeometry() does O(1)-per-element CPU
    // vector math to decide *what* to draw; these methods do the actual
    // antialiased GPU rendering. Points are in local (pre-transform) space,
    // matching drawLine()/drawRect()/drawPath()'s existing convention. Each
    // respects `paint.blend_mode` via pipelineForBlendMode(), unlike
    // Metal's equivalents (that backend's line/rect pipelines are srcOver-
    // only today).
    // ------------------------------------------------------------------

    // One antialiased, butt-ended segment body via line_pipeline_.
    void drawStrokeSegmentBody(
        const Offset& p0, const Offset& p1, float half_width,
        const Paint& paint, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // One antialiased round cap/join, as a filled circle via rrect_pipeline_
    // (same mechanism as drawCircle()).
    void drawStrokeRoundPrimitive(
        const Offset& center, float half_width,
        const Paint& paint, const Matrix4& transform,
        campello_gpu::RenderPassEncoder& encoder);

    // Strokes a polyline as individual GPU draw calls (segment bodies +
    // round caps/joins via rrect_pipeline_ + bevel/miter wedges via
    // colored_quad_pipeline_). For drawLine()/drawRect() stroke, where
    // point count is always small.
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
        std::vector<ColoredQuadVertex>& verts);

    void runBlurPass(
        std::shared_ptr<campello_gpu::Texture> src,
        std::shared_ptr<campello_gpu::Texture> dst,
        float sigma, bool horizontal,
        campello_gpu::CommandEncoder& encoder);

    // Build a 256×1 RGBA LUT texture from gradient colors/stops.
    std::shared_ptr<campello_gpu::Texture> buildGradientLUT(
        const std::vector<Color>& colors,
        const std::vector<float>& stops);

    // `cached_bind_group`, if non-null, is reused as-is instead of building
    // a fresh BindGroup for `texture` (see Renderer::text_texture_cache_'s
    // doc comment). Returns the BindGroup actually used — either
    // `cached_bind_group` passed straight through, or a freshly built one
    // (nullptr if drawing was aborted, e.g. missing pipeline). Callers that
    // want to skip rebuilding it next time should keep it and pass it back
    // in.
    // `sampler`: nullptr (default) uses linear_sampler_, matching every
    // existing call site's behavior unchanged. drawImage() passes
    // nearest_sampler_ explicitly when the caller requests
    // FilterQuality::none. Only takes effect when `cached_bind_group` is
    // null (a supplied cache already has its own sampler baked in).
    std::shared_ptr<campello_gpu::BindGroup> drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>    texture,
        const QuadCorner&                         c00,
        const QuadCorner&                         c10,
        const QuadCorner&                         c01,
        const QuadCorner&                         c11,
        float                                     opacity,
        const Rect&                               clip,
        campello_gpu::RenderPassEncoder&          encoder,
        std::shared_ptr<campello_gpu::BindGroup>  cached_bind_group = nullptr,
        bool                                      persistent        = false,
        std::shared_ptr<campello_gpu::Sampler>    sampler            = nullptr);

    // Mirrors drawTexturedQuad() (the non-axis-aligned, vertex-buffer path
    // only — icons are drawn few-per-frame, so the axis-aligned push-
    // constant-only fast path isn't worth a second variant), but binds
    // icon_pipeline_ (samples only the source texture's alpha channel and
    // recolors with `tint` — see icon.frag) instead of quad_pipeline_.
    // `sampler`: see drawTexturedQuad()'s doc comment.
    void drawTintedTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>    texture,
        const QuadCorner&                         c00,
        const QuadCorner&                         c10,
        const QuadCorner&                         c01,
        const QuadCorner&                         c11,
        const Color&                               tint,
        float                                     opacity,
        const Rect&                               clip,
        campello_gpu::RenderPassEncoder&          encoder,
        std::shared_ptr<campello_gpu::Sampler>    sampler = nullptr);

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  colored_quad_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  rect_aa_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  vertices_pipeline_;

    // Path::fillType() stencil-then-cover pipelines -- see their creation
    // block's doc comment in the constructor.
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_write_winding_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_write_evenodd_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  path_fill_stencil_cover_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  rrect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  line_pipeline_;

    // Blend-mode variants of the solid-color pipelines. Only a subset of
    // BlendMode is supported by the fixed-function blend state; unsupported
    // modes fall back to srcOver.
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> rect_blend_pipelines_;
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> colored_quad_blend_pipelines_;
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> rect_aa_blend_pipelines_;
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> vertices_blend_pipelines_;
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> rrect_blend_pipelines_;
    std::map<BlendMode, std::shared_ptr<campello_gpu::RenderPipeline>> line_blend_pipelines_;

    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_aa_pipeline_;
    // Icon pipeline (tinted template images — see icon.vert/icon.frag).
    // Reuses quad_bgl_ (same texture@1 + sampler@2 binding shape) but
    // needs its own pipeline layout: IconUniforms must be readable from
    // the fragment stage too (for `tint`), unlike quad_layout_'s
    // vertex-only push constant range.
    std::shared_ptr<campello_gpu::RenderPipeline>  icon_pipeline_;
    std::shared_ptr<campello_gpu::PipelineLayout>  icon_layout_;
    std::shared_ptr<campello_gpu::RenderPipeline>  clip_shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  shader_mask_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  blur_pipeline_;
    // Intermediate textures for the two-pass Gaussian blur.
    std::shared_ptr<campello_gpu::Texture>         blur_h_tex_;
    std::shared_ptr<campello_gpu::Texture>         blur_v_tex_;
    uint32_t                                        blur_tex_w_ = 0;
    uint32_t                                        blur_tex_h_ = 0;
    std::shared_ptr<campello_gpu::BindGroupLayout> quad_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout> shader_mask_bgl_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rrect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  line_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  quad_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  shader_mask_layout_;
    std::shared_ptr<campello_gpu::Sampler>          linear_sampler_;

    // Nearest-neighbor counterpart to linear_sampler_, selected by
    // drawImage()/drawTintedImage() only, per-draw, when the caller passes
    // FilterQuality::none.
    std::shared_ptr<campello_gpu::Sampler>          nearest_sampler_;
    // Two pools, one per fixed vertex-struct size — see UniformBufferPool's
    // doc comment. colored_quad_vertex_pool_ backs drawFilledQuad()'s
    // ColoredQuadVertex; quad_vertex_pool_ backs drawTexturedQuad()'s and
    // drawClipShapeComposite()'s (shared) QuadVertex.
    UniformBufferPool colored_quad_vertex_pool_;
    UniformBufferPool quad_vertex_pool_;

    // drawVertices()'s own vertex (VerticesVertex arrays) and index
    // (uint32_t arrays) buffers — see IndexBufferPool's doc comment for why
    // the index buffer needs its own pool class, not just its own instance.
    UniformBufferPool vertices_vertex_pool_;
    IndexBufferPool    vertices_index_pool_;
    // Per-frame resources for the frame currently being recorded.
    std::vector<std::shared_ptr<campello_gpu::Buffer>>      frame_buffers_;
    std::vector<std::shared_ptr<campello_gpu::Texture>>     frame_textures_;
    // TextureViews used as offscreen render targets: vkCmdBeginRenderingKHR
    // records the raw VkImageView — it must remain valid until vkQueueSubmit.
    std::vector<std::shared_ptr<campello_gpu::TextureView>> frame_views_;
    // setViewport()'s doc comment explains why these two extra generations
    // exist (campello_gpu's Vulkan Device::submit() is no longer blocking).
    std::vector<std::shared_ptr<campello_gpu::Buffer>>      prev_frame_buffers_;
    std::vector<std::shared_ptr<campello_gpu::Texture>>     prev_frame_textures_;
    std::vector<std::shared_ptr<campello_gpu::TextureView>> prev_frame_views_;
    std::vector<std::shared_ptr<campello_gpu::Buffer>>      prev2_frame_buffers_;
    std::vector<std::shared_ptr<campello_gpu::Texture>>     prev2_frame_textures_;
    std::vector<std::shared_ptr<campello_gpu::TextureView>> prev2_frame_views_;

    std::unique_ptr<ITextRasterizer>               text_rasterizer_;

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;
    float dpr_  = 1.0f;

    // Scissor caching
    float last_scissor_x_ = -1.0f;
    float last_scissor_y_ = -1.0f;
    float last_scissor_w_ = -1.0f;
    float last_scissor_h_ = -1.0f;
};

} // namespace systems::leal::campello_widgets
