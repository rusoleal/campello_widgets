#pragma once

#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/draw_command.hpp>

namespace systems::leal::campello_gpu { class RenderPassEncoder; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief Per-frame context passed to RenderObject::paint().
     *
     * PaintContext is the high-level companion to Canvas. While Canvas owns
     * the recording of draw commands and transform/clip state, PaintContext
     * carries the frame-level GPU encoder and viewport dimensions, and will
     * grow to manage compositing layers as the rendering pipeline matures.
     *
     * RenderObjects that only need to draw should operate on `canvas()`.
     * Container render objects that need to recurse into children pass the
     * full PaintContext down the tree.
     *
     * **Thread safety:** PaintContext is not thread-safe. Use only on the
     * render thread.
     */
    class PaintContext
    {
    public:
        /**
         * @param encoder        The active render-pass encoder for this frame.
         * @param viewport_width  Viewport width in logical pixels.
         * @param viewport_height Viewport height in logical pixels.
         */
        PaintContext(
            campello_gpu::RenderPassEncoder& encoder,
            float viewport_width,
            float viewport_height);

        ~PaintContext() = default;

        // Non-copyable, non-movable — tied to a single render-pass lifetime.
        PaintContext(const PaintContext&)            = delete;
        PaintContext& operator=(const PaintContext&) = delete;

        // ------------------------------------------------------------------
        // Canvas access
        // ------------------------------------------------------------------

        /**
         * @brief The canvas for this paint pass.
         *
         * Use this to issue draw commands (drawRect, drawText, drawImage) and
         * to manage transform/clip state via save/restore.
         */
        Canvas& canvas() noexcept { return canvas_; }
        const Canvas& canvas() const noexcept { return canvas_; }

        // ------------------------------------------------------------------
        // Frame-level accessors
        // ------------------------------------------------------------------

        campello_gpu::RenderPassEncoder& encoder() noexcept { return encoder_; }

        float viewportWidth()  const noexcept { return viewport_width_;  }
        float viewportHeight() const noexcept { return viewport_height_; }

        /**
         * @brief The accumulated draw commands for this paint pass.
         *
         * Forwarded from canvas(). The Renderer reads this list after
         * performPaint() returns and dispatches each command to the GPU backend.
         */
        const DrawList& commands() const noexcept { return canvas_.commands(); }

    private:
        campello_gpu::RenderPassEncoder& encoder_;

        float  viewport_width_;
        float  viewport_height_;
        Canvas canvas_;
    };

} // namespace systems::leal::campello_widgets
