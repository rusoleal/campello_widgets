#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
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

    void drawText(
        const DrawTextCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    void drawImage(
        const DrawImageCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    Size measureText(const TextSpan& span) const override;

    // ------------------------------------------------------------------
    // Offscreen / compositing (BackdropFilter + ShaderMask)
    // ------------------------------------------------------------------

    campello_gpu::PixelFormat offscreenPixelFormat() const noexcept override
    {
        return pixel_format_;
    }

    std::shared_ptr<campello_gpu::Texture> createOffscreenTexture(
        uint32_t width, uint32_t height) override;

    std::shared_ptr<campello_gpu::RenderPassEncoder> beginOffscreenPass(
        std::shared_ptr<campello_gpu::Texture> tex,
        campello_gpu::CommandEncoder&          encoder) override;

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
        campello_gpu::RenderPassEncoder&       encoder) override;

    // ------------------------------------------------------------------

    void setViewport(float w, float h) noexcept override
    {
        vp_w_ = w;
        vp_h_ = h;
        last_scissor_x_ = last_scissor_y_ = last_scissor_w_ = last_scissor_h_ = -1.0f;
        ++frame_counter_;
        evictStaleTextTextures();
        rect_uniform_pool_.beginFrame();
        shape_uniform_pool_.beginFrame();
        line_uniform_pool_.beginFrame();
        quad_uniform_pool_.beginFrame();
    }
    void setDevicePixelRatio(float dpr) noexcept { dpr_ = dpr; }

    /** Returns true if all render pipelines were successfully compiled. */
    bool isValid() const noexcept { return rect_pipeline_ != nullptr; }

private:
    // Returns false if the clip is empty — callers must skip the draw in that case.
    bool applyScissor(const Rect& clip, campello_gpu::RenderPassEncoder& encoder);

    void drawFilledRect(
        float x, float y, float w, float h,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawShape(
        float x, float y, float w, float h,
        float corner_r, float stroke_w, float kind,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    // `cached_bind_group`, if non-null, is reused as-is instead of building a
    // fresh BindGroup for `texture` (see "Text texture cache" below — the
    // bind group is created once alongside the texture and stays valid for
    // as long as the cache entry does).
    void drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        float dst_x, float dst_y, float dst_w, float dst_h,
        float src_u0, float src_v0, float src_u1, float src_v1,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder,
        std::shared_ptr<campello_gpu::BindGroup> cached_bind_group = nullptr);

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
    // Text texture cache
    //
    // CoreText rasterization + GPU texture allocation/upload in drawText()
    // is expensive (CPU layout pass + bitmap render + texture creation).
    // Caching the resulting texture per (text, style) lets unchanged text
    // reuse the same GPU texture across frames instead of redoing all of
    // that work every frame, even for text that never visually changes.
    // The BindGroup is cached alongside the texture (same lifetime, same
    // eviction) so a cache hit also skips Device::createBindGroup().
    // ------------------------------------------------------------------

    struct TextTextureCacheEntry
    {
        std::shared_ptr<campello_gpu::Texture>  texture;
        std::shared_ptr<campello_gpu::BindGroup> bind_group;
        uint32_t                                width  = 0;
        uint32_t                                height = 0;
        uint64_t                                last_used_frame = 0;
    };

    struct TextSpanHash
    {
        size_t operator()(const TextSpan& s) const noexcept;
    };

    // Looks up (or rasterizes and inserts) the texture for `span`, marking
    // it as used on the current frame. Returns nullptr if rasterization
    // produced an empty/invalid result.
    const TextTextureCacheEntry* lookupOrCreateTextTexture(const TextSpan& span);

    // Drops cache entries that weren't drawn in the last kTextTextureMaxAgeFrames
    // frames, so text that's no longer on screen (removed widgets, dynamic
    // text whose value keeps changing) doesn't grow the cache unboundedly.
    // Called once per frame from setViewport().
    void evictStaleTextTextures();

    static constexpr uint64_t kTextTextureMaxAgeFrames = 120;

    std::unordered_map<TextSpan, TextTextureCacheEntry, TextSpanHash> text_texture_cache_;
    uint64_t                                                          frame_counter_ = 0;

    UniformBufferPool rect_uniform_pool_;
    UniformBufferPool shape_uniform_pool_;
    UniformBufferPool line_uniform_pool_;
    UniformBufferPool quad_uniform_pool_;

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  line_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  quad_bgl_;
    std::shared_ptr<campello_gpu::Sampler>          quad_sampler_;

    // Blur pipeline (reuses quad_bgl_ for texture+sampler binding).
    std::shared_ptr<campello_gpu::RenderPipeline>  blur_pipeline_;

    // ShaderMask pipeline (child tex + LUT tex + sampler).
    std::shared_ptr<campello_gpu::RenderPipeline>  shader_mask_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  shader_mask_bgl_;

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
