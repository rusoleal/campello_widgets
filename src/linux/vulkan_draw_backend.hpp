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
// Supported:  drawRect, drawRRect, drawImage, drawText (FreeType + HarfBuzz),
//             createOffscreenTexture, beginOffscreenPass, drawClipShapeComposite
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
        frame_views_.clear();
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

    // Per-vertex projected position + UV — mirrors Metal's ProjectedCorner.
    struct QuadCorner { float x, y, w, u, v; };

private:
    void applyScissor(
        const Rect& clip,
        campello_gpu::RenderPassEncoder& encoder);

    void runBlurPass(
        std::shared_ptr<campello_gpu::Texture> src,
        std::shared_ptr<campello_gpu::Texture> dst,
        float sigma, bool horizontal,
        campello_gpu::CommandEncoder& encoder);

    void drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>    texture,
        const QuadCorner&                         c00,
        const QuadCorner&                         c10,
        const QuadCorner&                         c01,
        const QuadCorner&                         c11,
        float                                     opacity,
        const Rect&                               clip,
        campello_gpu::RenderPassEncoder&          encoder);

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    campello_gpu::PixelFormat                      pixel_format_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  colored_quad_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  rrect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  clip_shape_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  blur_pipeline_;
    // Intermediate textures for the two-pass Gaussian blur.
    std::shared_ptr<campello_gpu::Texture>         blur_h_tex_;
    std::shared_ptr<campello_gpu::Texture>         blur_v_tex_;
    uint32_t                                        blur_tex_w_ = 0;
    uint32_t                                        blur_tex_h_ = 0;
    std::shared_ptr<campello_gpu::BindGroupLayout> uniforms_bgl_;
    std::shared_ptr<campello_gpu::BindGroupLayout> quad_bgl_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  rrect_layout_;
    std::shared_ptr<campello_gpu::PipelineLayout>  quad_layout_;
    std::shared_ptr<campello_gpu::Sampler>          linear_sampler_;
    // Per-frame resources kept alive until the start of the next frame.
    // vkQueueWaitIdle in Device::submit() ensures the GPU is done with the
    // current frame before setViewport() clears these on the next frame.
    std::vector<std::shared_ptr<campello_gpu::Buffer>>      frame_buffers_;
    std::vector<std::shared_ptr<campello_gpu::Texture>>     frame_textures_;
    // TextureViews used as offscreen render targets: vkCmdBeginRenderingKHR
    // records the raw VkImageView — it must remain valid until vkQueueSubmit.
    std::vector<std::shared_ptr<campello_gpu::TextureView>> frame_views_;

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
