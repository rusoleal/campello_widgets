#include <campello_widgets/ui/draw_backend_factory.hpp>

#if defined(__APPLE__)
#include "gpu/metal/metal_draw_backend.hpp"
#endif

namespace systems::leal::campello_widgets
{

std::unique_ptr<IDrawBackend> createDrawBackend(
    std::shared_ptr<campello_gpu::Device> device,
    Color bg_color,
    campello_gpu::PixelFormat pixel_format)
{
#if defined(__APPLE__)
    return std::make_unique<MetalDrawBackend>(std::move(device), bg_color, pixel_format);
#else
    (void)device; (void)bg_color; (void)pixel_format;
    return nullptr;
#endif
}

} // namespace systems::leal::campello_widgets
