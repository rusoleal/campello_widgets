#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <campello_widgets/ui/color.hpp>
#include <memory>

namespace systems::leal::campello_gpu { class Device; }

namespace systems::leal::campello_widgets::testing
{

/**
 * @brief Creates the platform-specific IDrawBackend used for offscreen GPU
 * capture (visual fidelity tests, thematic component harness).
 *
 * This constructs the same production backend (MetalDrawBackend,
 * VulkanDrawBackend, D3DDrawBackend) that real running apps use — there is
 * no test-only rendering path. Implemented in the platform-specific source
 * file. Returns nullptr if the backend cannot be created or its pipelines
 * fail to compile.
 */
std::unique_ptr<IDrawBackend> createOffscreenDrawBackend(
    std::shared_ptr<campello_gpu::Device> device,
    Color                                 bg_color,
    campello_gpu::PixelFormat             pixel_format);

} // namespace systems::leal::campello_widgets::testing
