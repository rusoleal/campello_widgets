#pragma once

#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_gpu/constants/pixel_format.hpp>
#include <memory>

namespace systems::leal::campello_gpu { class Device; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief Constructs the platform-appropriate `IDrawBackend` for `device`.
     *
     * `Renderer` draws nothing at all until a backend is attached via
     * `setDrawBackend()` (see that method's doc comment) -- normally done by
     * each platform's own bootstrap (e.g. macos/run_app.mm constructing a
     * `MetalDrawBackend`), which lives in a private, platform-specific header
     * not exposed to consumers outside this library.
     *
     * This is that same construction, exposed publicly for callers that run
     * a `Renderer`/`RenderBox` standalone -- with no App/Window of their own
     * to go through the normal bootstrap path (see `Renderer`'s own doc
     * comment: "the Renderer only requires a root RenderBox... how that
     * render object is populated... is left to the caller"). A `DrawBackend`
     * is the other half such a caller needs.
     *
     * @return nullptr on a platform with no backend implementation yet.
     */
    std::unique_ptr<IDrawBackend> createDrawBackend(
        std::shared_ptr<campello_gpu::Device> device,
        Color bg_color,
        campello_gpu::PixelFormat pixel_format);

} // namespace systems::leal::campello_widgets
