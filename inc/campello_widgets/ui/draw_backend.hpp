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
    class BindGroup;
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
         * Must be called exactly once per real frame, before renderFrame().
         * Besides recording (w, h), this advances per-frame bookkeeping
         * (uniform-buffer pool generations, the text-texture-cache eviction
         * counter, the scissor-redundancy cache) — calling it more than once
         * within a single still-being-encoded frame wraps that bookkeeping
         * around and can make an in-flight draw's pooled uniform buffer get
         * overwritten by a later draw before the GPU ever reads it. Code
         * that needs to temporarily point coordinate transforms at a
         * differently-sized offscreen target mid-frame (e.g. ClipRRect/
         * ShaderMask's offscreen composites) must use setViewportSize()
         * instead, which only updates the transform-facing (w, h).
         */
        virtual void setViewport(float /*w*/, float /*h*/) noexcept {}

        /**
         * @brief Updates only the (w, h) used for NDC/scissor conversion,
         * without the once-per-frame bookkeeping setViewport() performs.
         *
         * Safe to call multiple times within a single frame — e.g. to
         * temporarily target a smaller offscreen texture during a
         * ClipRRect/ClipOval/ShaderMask composite, then restore the
         * frame's real viewport size afterward.
         */
        virtual void setViewportSize(float /*w*/, float /*h*/) noexcept {}

        /**
         * @brief Set the device pixel ratio for the current frame.
         *
         * Called from Renderer::rasterFrame() (the raster phase) with the
         * DPR value frozen into that frame's FramePackage — backends that
         * cache rasterized content keyed partly by physical-pixel size
         * (e.g. a glyph/text-texture cache) must treat this as exclusively
         * raster-phase state, not read it from elsewhere.
         */
        virtual void setDevicePixelRatio(float /*dpr*/) noexcept {}

        /**
         * @brief Called at the start of each flushDrawList() invocation.
         *
         * Each call to flushDrawList() follows a beginRenderPass(), which
         * resets GPU dynamic state (viewport, scissor). Implementations that
         * cache GPU state (e.g. scissor rect) must invalidate their cache here
         * so that the first draw command after beginRenderPass always re-emits
         * the necessary GPU state.
         */
        virtual void onBeginFlush() noexcept {}

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
         * @brief Rasterizes `span` to a new GPU texture using the platform's
         * native text system (GDI/CoreText/FreeType+HarfBuzz).
         *
         * No caching — callers that want caching do it themselves (see
         * Renderer::text_texture_cache_, the sole caller of this method).
         * Returns nullptr on failure (`out_width`/`out_height` are then
         * unspecified).
         *
         * @param span      Text content + style to rasterize.
         * @param dpr       Device pixel ratio — rasterize at physical
         *                  resolution, matching what drawTextTexture()'s
         *                  `transform` will project the quad through.
         * @param out_width  Rasterized texture width, in pixels.
         * @param out_height Rasterized texture height, in pixels.
         */
        virtual std::shared_ptr<campello_gpu::Texture> rasterizeText(
            const TextSpan& /*span*/, float /*dpr*/,
            uint32_t& /*out_width*/, uint32_t& /*out_height*/) { return nullptr; }

        /**
         * @brief Draws a previously-rasterized text texture (from
         * rasterizeText()) as a quad at `origin`.
         *
         * @param texture           The rasterized glyph-bitmap texture.
         * @param cached_bind_group A bind group this same `texture` was
         *                          already drawn with, if any (nullptr on a
         *                          cache miss — the implementation builds
         *                          one). Passing the previous call's return
         *                          value back in on every subsequent draw
         *                          of the same texture skips rebuilding it.
         * @param width             Texture width, in pixels (from
         *                          rasterizeText()'s `out_width`).
         * @param height            Texture height, in pixels.
         * @param origin            Top-left of the text quad, in the
         *                          current transform's coordinate space.
         * @param transform         Effective transform matrix.
         * @param clip              Effective clip rectangle.
         * @param encoder           Active render pass encoder.
         * @return The bind group actually used — either `cached_bind_group`
         *         passed straight through, or a freshly built one. Callers
         *         that want to skip rebuilding next time should store this
         *         and pass it back in as `cached_bind_group`.
         */
        virtual std::shared_ptr<campello_gpu::BindGroup> drawTextTexture(
            std::shared_ptr<campello_gpu::Texture>   /*texture*/,
            std::shared_ptr<campello_gpu::BindGroup> /*cached_bind_group*/,
            uint32_t /*width*/, uint32_t /*height*/,
            const Offset&                     /*origin*/,
            const Matrix4&                    /*transform*/,
            const Rect&                       /*clip*/,
            campello_gpu::RenderPassEncoder&  /*encoder*/) { return nullptr; }

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
            const Rect&                                   /*clip*/,
            campello_gpu::RenderPassEncoder&              /*encoder*/) {}

        /**
         * @brief Composites `child_tex` through a rounded-rect/oval SDF mask.
         *
         * `child_tex` contains ClipRRect/ClipOval's children rendered to an
         * offscreen buffer. The implementation evaluates a signed-distance
         * mask (rounded rect when `is_oval` is false, ellipse when true) over
         * `bounds` and draws `child_tex * mask_alpha` at `bounds` in the
         * current render pass. `corner_radius` is in the same (logical)
         * units as `bounds` and is ignored when `is_oval` is true. `clip` is
         * the ancestor clip in effect where the ClipRRect/ClipOval was
         * encountered (e.g. a scrollable's viewport) — without applying it,
         * the composited quad ignores any ancestor clipping, so content
         * (e.g. an avatar image, a grid cell) can bleed past a scrolled
         * list's edge into whatever is painted above it.
         */
        virtual void drawClipShapeComposite(
            std::shared_ptr<campello_gpu::Texture>        /*child_tex*/,
            const Rect&                                   /*bounds*/,
            float                                          /*corner_radius*/,
            bool                                           /*is_oval*/,
            const Matrix4&                                /*transform*/,
            const Rect&                                   /*clip*/,
            campello_gpu::RenderPassEncoder&              /*encoder*/) {}
    };

} // namespace systems::leal::campello_widgets
