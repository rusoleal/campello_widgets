// Metal backend factory for offscreen GPU capture (macOS/iOS).
#include "offscreen_draw_backend.hpp"
#include "../gpu/metal/metal_draw_backend.hpp"
#include <iostream>

namespace cwt = systems::leal::campello_widgets::testing;
namespace cw  = systems::leal::campello_widgets;
namespace GPU = systems::leal::campello_gpu;

std::unique_ptr<cw::IDrawBackend> cwt::createOffscreenDrawBackend(
    std::shared_ptr<GPU::Device> device,
    cw::Color                    bg_color,
    GPU::PixelFormat             pixel_format)
{
    auto backend = std::make_unique<cw::MetalDrawBackend>(
        std::move(device),
        bg_color,
        pixel_format);

    if (!backend->isValid()) {
        std::cerr << "[offscreen_draw_backend] MetalDrawBackend pipelines failed to compile\n";
        return nullptr;
    }
    return backend;
}
