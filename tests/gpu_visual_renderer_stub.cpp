// Fallback stub for platforms without a GpuVisualRenderer backend.
#include "gpu_visual_renderer_backend.hpp"

namespace cwt = systems::leal::campello_widgets::testing;
namespace cw  = systems::leal::campello_widgets;
namespace GPU = systems::leal::campello_gpu;

std::unique_ptr<cw::IDrawBackend> cwt::createVisualRendererBackend(
    std::shared_ptr<GPU::Device> /*device*/,
    cw::Color                    /*bg_color*/,
    GPU::PixelFormat             /*pixel_format*/)
{
    return nullptr;
}
