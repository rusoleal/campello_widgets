#pragma once

#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/color.hpp>

#include <cstdint>
#include <memory>

namespace systems::leal::campello_gpu
{
    class TextureView;
}

namespace systems::leal::campello_widgets
{

    /**
     * @brief Immutable snapshot of everything the raster phase needs for one frame.
     *
     * Produced by `Renderer::buildFrame()` on the UI thread; consumed by
     * `Renderer::rasterFrame()`, which may run on a separate raster thread.
     * Nothing in this struct is written after construction — that is the
     * whole thread-safety contract of the UI/raster split: the raster phase
     * never reads a live, UI-thread-mutable `Renderer` member, only the
     * values frozen into this package at `buildFrame()`-return time.
     */
    struct FramePackage
    {
        DrawList draw_list;

        float viewport_width  = 0.0f;
        float viewport_height = 0.0f;
        float device_pixel_ratio = 1.0f;
        Color clear_color;
        // See Renderer's constructor doc comment on `clear_target`.
        bool  clear_target = true;

        bool  has_backdrop_filter = false;
        float max_sigma_x = 0.0f;
        float max_sigma_y = 0.0f;

        std::shared_ptr<campello_gpu::TextureView> target;

        // Owning handle for a platform-native drawable (e.g. CAMetalDrawable)
        // that must stay alive until raster has presented it. Platform code
        // is responsible for wrapping the native handle in a shared_ptr with
        // an appropriate release deleter; campello_widgets treats this as
        // opaque and only keeps it alive for the package's lifetime.
        std::shared_ptr<void> retained_drawable;

        uint64_t frame_id = 0; // diagnostics only
    };

} // namespace systems::leal::campello_widgets
