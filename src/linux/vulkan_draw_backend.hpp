#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <memory>
#include <vector>

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class PipelineLayout;
    class Sampler;
    class Texture;
    class Buffer;
}

namespace systems::leal::campello_widgets
{

class LinuxTextRasterizer;

// ---------------------------------------------------------------------------
// VulkanDrawBackend
//
// IDrawBackend implementation for Linux/Vulkan.
// Uses campello_gpu's public API with pre-compiled SPIR-V shaders.
//
// Supported:  drawRect, drawRRect, drawImage, drawText (FreeType + HarfBuzz)
// No-op:      drawCircle, drawOval, drawLine,
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

    void drawText(
        const DrawTextCmd&               cmd,
        const Matrix4&                   transform,
        const Rect&                      clip,
        campello_gpu::RenderPassEncoder& encoder) override;

    Size measureText(const TextSpan& span) const override;

    void setViewport(float w, float h) noexcept override
    {
        // Safe to clear previous frame's buffers here: the renderer calls
        // vkQueueWaitIdle inside submit() before returning, so the GPU is
        // done with the prior frame by the time this is called again.
        frame_buffers_.clear();
        frame_textures_.clear();
        vp_w_ = w; vp_h_ = h;
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

private:
    void applyScissor(
        const Rect& clip,
        campello_gpu::RenderPassEncoder& encoder);

    void drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>    texture,
        const Rect&                               dst_rect,
        const Rect&                               src_rect,
        float                                     opacity,
        const Rect&                               clip,
        campello_gpu::RenderPassEncoder&          encoder);

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  rrect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout> uniforms_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout> quad_bgl_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rrect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  quad_layout_;
    std::shared_ptr<campello_gpu::Sampler>          linear_sampler_;
    // Per-frame uniform buffers kept alive until the start of the next frame
    // (vkQueueWaitIdle in submit() ensures GPU is done before they're cleared).
    std::vector<std::shared_ptr<campello_gpu::Buffer>>  frame_buffers_;
    std::vector<std::shared_ptr<campello_gpu::Texture>> frame_textures_;

    std::unique_ptr<LinuxTextRasterizer>           text_rasterizer_;

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;

    // Scissor caching
    float last_scissor_x_ = -1.0f;
    float last_scissor_y_ = -1.0f;
    float last_scissor_w_ = -1.0f;
    float last_scissor_h_ = -1.0f;
};

} // namespace systems::leal::campello_widgets
