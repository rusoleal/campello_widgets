// Vulkan backend factory for offscreen GPU capture (Linux / Android).
#include "offscreen_draw_backend.hpp"
#include "../gpu/vulkan/vulkan_draw_backend.hpp"

namespace cwt = systems::leal::campello_widgets::testing;
namespace cw  = systems::leal::campello_widgets;
namespace GPU = systems::leal::campello_gpu;

std::unique_ptr<cw::IDrawBackend> cwt::createOffscreenDrawBackend(
    std::shared_ptr<GPU::Device> device,
    cw::Color                    bg_color,
    GPU::PixelFormat             pixel_format)
{
    return std::make_unique<cw::VulkanDrawBackend>(
        std::move(device),
        bg_color,
        pixel_format);
}
