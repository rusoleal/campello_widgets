#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <memory>

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class BindGroup;
    class PipelineLayout;
    class Sampler;
    class Texture;
}

namespace systems::leal::campello_widgets
{

class AndroidTextRasterizer;

// ---------------------------------------------------------------------------
// VulkanDrawBackend
//
// IDrawBackend implementation for Android/Vulkan.
// Uses campello_gpu's public API with pre-compiled SPIR-V shaders.
//
// Supported:  drawRect, drawImage, text (Android Canvas/Paint via JNI,
//             rasterizeText/drawTextTexture — cached by Renderer)
// No-op:      drawCircle, drawOval, drawRRect, drawLine,
//             blurTexture, drawBackdropFilter, drawShaderMaskComposite
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

    void drawImage(
        const DrawImageCmd&              cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

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

    void setViewport(float w, float h) noexcept override { vp_w_ = w; vp_h_ = h; }

    void onBeginFlush() noexcept override
    {
        last_scissor_x_ = -1.0f;
        last_scissor_y_ = -1.0f;
        last_scissor_w_ = -1.0f;
        last_scissor_h_ = -1.0f;
    }

    campello_gpu::PixelFormat offscreenPixelFormat() const noexcept override
    {
        return pixel_format_;
    }

private:
    void applyScissor(
        const Rect& clip,
        campello_gpu::RenderPassEncoder& encoder);

    // `cached_bind_group`, if non-null, is reused as-is for the texture+
    // sampler bind group instead of building a fresh one (see
    // Renderer::text_texture_cache_'s doc comment) — safe here because,
    // unlike the Linux/Vulkan backend, this bind group only ever holds
    // texture@1/sampler@2, with per-draw uniforms in a separate bind group
    // (u_bind_group below) that's always rebuilt fresh. Returns the
    // BindGroup actually used — either `cached_bind_group` passed straight
    // through, or a freshly built one (nullptr if drawing was aborted).
    std::shared_ptr<campello_gpu::BindGroup> drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>    texture,
        const Rect&                               dst_rect,
        const Rect&                               src_rect,
        float                                     opacity,
        const Rect&                               clip,
        campello_gpu::RenderPassEncoder&          encoder,
        std::shared_ptr<campello_gpu::BindGroup>  cached_bind_group = nullptr);

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout> uniforms_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout> quad_bgl_;
    std::shared_ptr<campello_gpu::Sampler>          linear_sampler_;

    std::unique_ptr<AndroidTextRasterizer>         text_rasterizer_;

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;

    // Scissor caching
    float last_scissor_x_ = -1.0f;
    float last_scissor_y_ = -1.0f;
    float last_scissor_w_ = -1.0f;
    float last_scissor_h_ = -1.0f;
};

} // namespace systems::leal::campello_widgets
