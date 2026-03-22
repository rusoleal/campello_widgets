#pragma once

#include <memory>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/text_span.hpp>

namespace systems::leal::campello_gpu { class RenderPassEncoder; }

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
    };

} // namespace systems::leal::campello_widgets
