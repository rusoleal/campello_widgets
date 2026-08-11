// DirectX 12 backend factory for GpuVisualRenderer (Windows).
#include "gpu_visual_renderer_backend.hpp"
#include "../windows/d3d_draw_backend.hpp"

namespace cwt = systems::leal::campello_widgets::testing;
namespace cw  = systems::leal::campello_widgets;
namespace GPU = systems::leal::campello_gpu;

std::unique_ptr<cw::IDrawBackend> cwt::createVisualRendererBackend(
    std::shared_ptr<GPU::Device> device,
    cw::Color                    bg_color,
    GPU::PixelFormat             pixel_format)
{
    return std::make_unique<cw::D3DDrawBackend>(
        std::move(device),
        bg_color,
        pixel_format);
}
