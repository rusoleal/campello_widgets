#pragma once

#include <memory>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

namespace systems::leal::campello_gpu
{
    class RenderPassEncoder;
    class CommandEncoder;
    class Texture;
}

namespace systems::leal::campello_widgets
{

    /**
     * @brief Interface for platform-specific GPU primitive drawing.
     *
     * IDrawBackend receives high-level draw commands from the Renderer and
     * translates them into campello_gpu pipeline calls. Each platform
     * (macOS/Metal, Windows/DX12, Android/Vulkan) provides its own
     * implementation with precompiled shader pipelines.
     *
     * Register an implementation with `Renderer::setDrawBackend()`.
     * Until a backend is registered the draw list is accumulated but
     * not executed on the GPU.
     *
     * Implementations must be thread-compatible with the render thread.
     */
    class IDrawBackend
    {
    public:
        virtual ~IDrawBackend() = default;

        /**
         * @brief Set the viewport dimensions for the current frame.
         *
         * Must be called before renderFrame() each frame so the backend
         * knows the output size for coordinate transforms.
         */
        virtual void setViewport(float /*w*/, float /*h*/) noexcept {}

        /**
         * @brief Draw a filled or stroked rectangle.
         *
         * @param cmd       The draw rect command (rect + paint).
         * @param transform Effective transform matrix at the time of the command.
         * @param clip      Effective clip rectangle at the time of the command.
         * @param encoder   Active render pass encoder for this frame.
         */
        virtual void drawRect(
            const DrawRectCmd&               cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) = 0;

        /**
         * @brief Draw a text span.
         *
         * @param cmd       The draw text command (span + origin).
         * @param transform Effective transform matrix.
         * @param clip      Effective clip rectangle.
         * @param encoder   Active render pass encoder.
         */
        virtual void drawText(
            const DrawTextCmd&               cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) = 0;

        /**
         * @brief Draw a GPU texture into a destination rectangle.
         *
         * @param cmd       The draw image command (texture + src/dst rects).
         * @param transform Effective transform matrix.
         * @param clip      Effective clip rectangle.
         * @param encoder   Active render pass encoder.
         */
        virtual void drawImage(
            const DrawImageCmd&              cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) = 0;

        /** @brief Draw a circle (fill or stroke). Default: no-op. */
        virtual void drawCircle(
            const DrawCircleCmd&             cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) { (void)cmd; (void)transform; (void)clip; (void)encoder; }

        /** @brief Draw an oval filling rect (fill or stroke). Default: no-op. */
        virtual void drawOval(
            const DrawOvalCmd&               cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) { (void)cmd; (void)transform; (void)clip; (void)encoder; }

        /** @brief Draw a rounded rectangle (fill or stroke). Default: no-op. */
        virtual void drawRRect(
            const DrawRRectCmd&              cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) { (void)cmd; (void)transform; (void)clip; (void)encoder; }

        /** @brief Draw a line segment. Default: no-op. */
        virtual void drawLine(
            const DrawLineCmd&               cmd,
            const Matrix4&                   transform,
            const Rect&                      clip,
            campello_gpu::RenderPassEncoder& encoder) { (void)cmd; (void)transform; (void)clip; (void)encoder; }

        /**
         * @brief Measures the bounding size of a text span using real font metrics.
         *
         * Called during the layout pass (no encoder available). The default
         * implementation returns Size::zero(); platform backends should override
         * this to query the native font system.
         */
        virtual Size measureText(const TextSpan& span) const
        {
            const float char_width  = span.style.font_size * 0.6f;
            const float line_height = span.style.font_size * 1.2f;
            return Size{ char_width * static_cast<float>(span.text.size()), line_height };
        }

        // ------------------------------------------------------------------
        // Offscreen / compositing support (BackdropFilter, ShaderMask)
        // ------------------------------------------------------------------

        /**
         * @brief Returns the pixel format used for offscreen render targets.
         *
         * The Renderer calls this when it needs to allocate backdrop/child
         * textures whose format must match the swapchain.
         */
        virtual campello_gpu::PixelFormat offscreenPixelFormat() const noexcept
        {
            return campello_gpu::PixelFormat::bgra8unorm;
        }

        /**
         * @brief Allocates (or reuses) an RGBA offscreen texture of the given size.
         *
         * The returned texture is suitable as a render-pass color attachment and
         * as a sampled texture binding.  Returns nullptr if allocation fails.
         */
        virtual std::shared_ptr<campello_gpu::Texture> createOffscreenTexture(
            uint32_t /*width*/, uint32_t /*height*/) { return nullptr; }

        /**
         * @brief Begins a render pass that targets `tex`.
         *
         * The pass clears the texture to transparent black.  The caller ends the
         * returned encoder when all child commands have been flushed.
         */
        virtual std::shared_ptr<campello_gpu::RenderPassEncoder> beginOffscreenPass(
            std::shared_ptr<campello_gpu::Texture> /*tex*/,
            campello_gpu::CommandEncoder&          /*encoder*/) { return nullptr; }

        /**
         * @brief Applies a separable Gaussian blur to `source` and returns the result.
         *
         * Runs two render passes (horizontal then vertical).  The resulting texture
         * is owned by the backend and lives until the next call to `blurTexture`.
         * Returns nullptr if the backend does not support blur.
         *
         * @param source    Texture to blur (must be readable as a sampled texture).
         * @param sigma_x   Horizontal blur radius (Gaussian sigma, pixels).
         * @param sigma_y   Vertical   blur radius (Gaussian sigma, pixels).
         * @param encoder   Active command encoder for recording the blur passes.
         */
        virtual std::shared_ptr<campello_gpu::Texture> blurTexture(
            std::shared_ptr<campello_gpu::Texture> /*source*/,
            float /*sigma_x*/, float /*sigma_y*/,
            campello_gpu::CommandEncoder& /*encoder*/) { return nullptr; }

        /**
         * @brief Draws the pre-blurred backdrop into the current render pass.
         *
         * `blurred_source` is the texture produced by `blurTexture()`.  The
         * implementation samples the region of `blurred_source` corresponding to
         * `cmd.bounds` and draws it as a full-viewport-coordinate textured quad.
         */
        virtual void drawBackdropFilter(
            const DrawBackdropFilterBeginCmd&             /*cmd*/,
            std::shared_ptr<campello_gpu::Texture>        /*blurred_source*/,
            const Matrix4&                                /*transform*/,
            const Rect&                                   /*clip*/,
            campello_gpu::RenderPassEncoder&              /*encoder*/) {}

        /**
         * @brief Composites `child_tex` with the gradient/shader mask from `cmd`.
         *
         * `child_tex` contains the children rendered to an offscreen buffer.
         * The implementation evaluates `cmd.shader` as a gradient mask, multiplies
         * it with `child_tex` according to `cmd.blend_mode`, and draws the result
         * at `cmd.bounds` in the current render pass.
         */
        virtual void drawShaderMaskComposite(
            std::shared_ptr<campello_gpu::Texture>        /*child_tex*/,
            const DrawShaderMaskBeginCmd&                 /*cmd*/,
            const Matrix4&                                /*transform*/,
            campello_gpu::RenderPassEncoder&              /*encoder*/) {}
    };

} // namespace systems::leal::campello_widgets
