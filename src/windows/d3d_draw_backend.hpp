#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <memory>

// Windows forward declarations
struct HWND__;
typedef HWND__* HWND;

namespace systems::leal::campello_gpu
{
    class Device;
    class RenderPipeline;
    class BindGroupLayout;
    class Sampler;
    class Texture;
}

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// D3DDrawBackend
//
// IDrawBackend implementation for Windows/Direct3D 11/12.
// Uses campello_gpu's public API to compile render pipelines.
//
// Call setViewport(w, h) once per frame before Renderer::renderFrame().
// ---------------------------------------------------------------------------
class D3DDrawBackend final : public IDrawBackend
{
public:
    D3DDrawBackend(
        std::shared_ptr<campello_gpu::Device> device,
        Color                                 bg_color,
        HWND                                  hwnd);

    ~D3DDrawBackend() override = default;

    void drawRect(
        const DrawRectCmd&               cmd,
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

    void setViewport(float w, float h) noexcept { vp_w_ = w; vp_h_ = h; }

private:
    void drawFilledRect(
        float x, float y, float w, float h,
        const Color& color,
        campello_gpu::RenderPassEncoder& encoder);

    void drawTexturedQuad(
        std::shared_ptr<campello_gpu::Texture>  texture,
        float dst_x, float dst_y, float dst_w, float dst_h,
        float src_u0, float src_v0, float src_u1, float src_v1,
        float opacity,
        campello_gpu::RenderPassEncoder&        encoder);

    std::shared_ptr<campello_gpu::Device>         device_;
    Color                                          bg_color_;
    HWND                                           hwnd_;

    std::shared_ptr<campello_gpu::RenderPipeline>  rect_pipeline_;
    std::shared_ptr<campello_gpu::RenderPipeline>  quad_pipeline_;
    std::shared_ptr<campello_gpu::BindGroupLayout>  quad_bgl_;
    std::shared_ptr<campello_gpu::Sampler>          quad_sampler_;

    float vp_w_ = 800.0f;
    float vp_h_ = 600.0f;
};

} // namespace systems::leal::campello_widgets
