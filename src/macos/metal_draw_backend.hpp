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
    class Sampler;
    class Texture;
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

    void setViewport(float w, float h) noexcept { vp_w_ = w; vp_h_ = h; }

    /** Returns true if all render pipelines were successfully compiled. */
    bool isValid() const noexcept { return rect_pipeline_ != nullptr; }

private:
    void drawFilledRect(
        float x, float y, float w, float h,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawShape(
        float x, float y, float w, float h,
        float corner_r, float stroke_w, float kind,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        float dst_x, float dst_y, float dst_w, float dst_h,
        float src_u0, float src_v0, float src_u1, float src_v1,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder);

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
};

} // namespace systems::leal::campello_widgets
