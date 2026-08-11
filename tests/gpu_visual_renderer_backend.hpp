#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <campello_widgets/ui/color.hpp>
#include <memory>

namespace systems::leal::campello_gpu { class Device; }

namespace systems::leal::campello_widgets::testing
{

/**
 * @brief Creates the platform-specific IDrawBackend used by GpuVisualRenderer.
 *
 * Implemented in the platform-specific source file (Metal, Vulkan, DirectX, or stub).
 * Returns nullptr if the backend cannot be created or its pipelines fail to compile.
 */
std::unique_ptr<IDrawBackend> createVisualRendererBackend(
    std::shared_ptr<campello_gpu::Device> device,
    Color                                 bg_color,
    campello_gpu::PixelFormat             pixel_format);

} // namespace systems::leal::campello_widgets::testing
