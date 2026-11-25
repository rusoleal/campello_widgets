#pragma once

#include <memory>
#include <campello_widgets/ui/render_image.hpp>
#include <campello_gpu/external_image.hpp>

namespace systems::leal::campello_widgets
{

    class VideoPlayerController;

    /**
     * @brief RenderBox that displays the currently-decoded frame of a
     * `VideoPlayerController`.
     *
     * Structurally parallel to `RenderDrawSurface`: a `RenderImage`
     * subclass that owns one persistent GPU texture, refreshed
     * incrementally rather than re-created every frame. Unlike
     * `RenderDrawSurface`, this class doesn't do the refreshing itself —
     * the attached `VideoPlayerController` drives it (see that class's
     * `attach()` doc comment for why it holds a pointer back), calling
     * `uploadFrame()` each time a new decoded frame is available and
     * `markNeedsPaint()` (inherited, public) directly afterward.
     */
    class RenderVideoPlayer : public RenderImage
    {
    public:
        explicit RenderVideoPlayer(std::shared_ptr<VideoPlayerController> controller);
        ~RenderVideoPlayer() override;

        void attach() override;
        void detach() override;

        void setController(std::shared_ptr<VideoPlayerController> controller);

        /**
         * @brief Uploads a decoded video frame into this object's dedicated
         * texture, allocating it first if this is the first frame or the
         * source's pixel dimensions changed.
         *
         * `width`/`height` are the frame's pixel dimensions; `bgra_data`
         * must be exactly `width * height * 4` bytes of tightly-packed
         * BGRA8 (no row padding) — the format `VideoPlayerController`'s
         * platform backend is expected to hand over. Called by the
         * attached `VideoPlayerController`, on the main thread, from its
         * own `TickerScheduler` subscription — never call this directly.
         *
         * Does *not* call `markNeedsPaint()` itself (the caller does, once,
         * after this returns) — matching `RenderDrawSurface::ensureSurface()`'s
         * split between allocating/updating a persistent texture and
         * triggering the repaint that displays it.
         */
        void uploadFrame(uint32_t width, uint32_t height, const void* bgra_data);

        /**
         * @brief Copies a decoder's frame straight into this object's
         * texture on the GPU (`Device::copyExternalImageToTexture()`: no CPU
         * readback, YUV converted in a shader), allocating the texture as
         * `uploadFrame()` does. `width`/`height` are the texture's size;
         * `options.uvTransform` can crop the source to it (e.g. a decoder's
         * crop rectangle).
         *
         * Returns false, leaving the texture as it was, when there's no
         * active backend or its device can't import `frame`'s type; the
         * caller then falls back to `uploadFrame()`. Same threading and
         * `markNeedsPaint()` rules as `uploadFrame()`.
         */
        bool copyExternalFrame(uint32_t width, uint32_t height,
                               const campello_gpu::ExternalImage& frame,
                               const campello_gpu::ExternalImageCopyOptions& options = {});

        /**
         * @brief `copyExternalFrame()` without waiting: the copy is
         * recorded into the next frame's command encoder (see
         * `Renderer::scheduleGpuUpload()`), so the UI thread doesn't block
         * on the GPU. What players call every video frame.
         *
         * `keep_alive` holds whatever must outlive the recording (e.g. a
         * retained CVPixelBuffer); it's dropped once the copy is recorded,
         * after which campello_gpu keeps the source alive until the GPU is
         * done. A source a decoder recycles (an AImage) must be kept by the
         * caller for a few more frames. Falls back to the synchronous copy
         * outside a frame build (no current Renderer). Returns false, like
         * `copyExternalFrame()`, when the frame can't be taken; failures
         * while recording are only logged.
         */
        bool queueExternalFrame(uint32_t width, uint32_t height,
                                const campello_gpu::ExternalImage& frame,
                                const campello_gpu::ExternalImageCopyOptions& options = {},
                                std::shared_ptr<void> keep_alive = nullptr);

        /// True when `copyExternalFrame()` can take frames of `type` from
        /// the active backend's device.
        static bool supportsExternalFrames(campello_gpu::ExternalImageType type);

    private:
        bool ensureTexture(uint32_t width, uint32_t height);

        std::shared_ptr<VideoPlayerController> controller_;
        std::shared_ptr<campello_gpu::Texture> video_texture_;
        uint32_t texture_width_  = 0;
        uint32_t texture_height_ = 0;
    };

} // namespace systems::leal::campello_widgets
