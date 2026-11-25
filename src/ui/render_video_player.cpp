#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/device.hpp>
#include <campello_gpu/command_encoder.hpp>
#include <campello_widgets/ui/renderer.hpp>

#include <algorithm>

#if defined(__ANDROID__)
#include <unistd.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace systems::leal::campello_widgets
{

    RenderVideoPlayer::RenderVideoPlayer(std::shared_ptr<VideoPlayerController> controller)
        : controller_(std::move(controller))
    {
    }

    RenderVideoPlayer::~RenderVideoPlayer()
    {
        if (controller_) controller_->detach(this);
    }

    void RenderVideoPlayer::attach()
    {
        if (controller_) controller_->attach(this);
    }

    void RenderVideoPlayer::detach()
    {
        if (controller_) controller_->detach(this);
    }

    void RenderVideoPlayer::setController(std::shared_ptr<VideoPlayerController> controller)
    {
        if (controller_ == controller) return;
        // Unconditional attach/detach, no "is this render object mounted"
        // check — updateRenderObject() (this method's only caller) is only
        // ever invoked on an already-live render object, exactly matching
        // RenderSingleChildScrollView::setController()'s identical
        // ScrollController handling.
        if (controller_) controller_->detach(this);
        controller_ = std::move(controller);
        if (controller_) controller_->attach(this);
    }

    bool RenderVideoPlayer::ensureTexture(uint32_t width, uint32_t height)
    {
        IDrawBackend* backend = RenderObject::activeBackend();
        if (!backend) return false;

        if (!video_texture_ || texture_width_ != width || texture_height_ != height)
        {
            // Dedicated (non-pooled) — this texture is kept and displayed
            // for the video's entire playback lifetime, unlike the
            // throwaway per-frame composites the backend's rotating pool
            // is designed for. See IDrawBackend::
            // createDedicatedOffscreenTexture()'s doc comment, and
            // RenderDrawSurface::ensureSurface()'s identical reasoning.
            video_texture_  = backend->createDedicatedOffscreenTexture(width, height);
            texture_width_  = width;
            texture_height_ = height;
            if (!video_texture_) return false;
            // Establishes texture_ identity once; further frames reuse the
            // same texture object and call markNeedsPaint() directly (the
            // caller's job — see uploadFrame()'s doc comment), since
            // RenderImage::setTexture() no-ops on an unchanged shared_ptr.
            setTexture(video_texture_);
        }
        return true;
    }

    void RenderVideoPlayer::uploadFrame(uint32_t width, uint32_t height, const void* bgra_data)
    {
        if (width == 0 || height == 0 || !bgra_data) return;
        if (!ensureTexture(width, height)) return;

        const uint64_t length = static_cast<uint64_t>(width) * height * 4;
        video_texture_->upload(0, length, const_cast<void*>(bgra_data));
    }

    bool RenderVideoPlayer::supportsExternalFrames(campello_gpu::ExternalImageType type)
    {
        IDrawBackend* backend = RenderObject::activeBackend();
        auto device = backend ? backend->gpuDevice() : nullptr;
        return device && device->supportsExternalImage(type);
    }

    namespace
    {
        // Frees what an ExternalImage hands to the copy (an Android fence,
        // a web handle) when no copy will take it.
        void releaseUnusedExternalImage(const campello_gpu::ExternalImage& frame)
        {
#if defined(__ANDROID__)
            if (frame.acquireFenceFd >= 0) close(frame.acquireFenceFd);
#elif defined(__EMSCRIPTEN__)
            if (frame.type == campello_gpu::ExternalImageType::webImageSource && frame.handle)
                EM_ASM({ if (Module['campelloGpu']) Module['campelloGpu']['releaseExternalImage']($0); },
                       static_cast<int>(reinterpret_cast<intptr_t>(frame.handle)));
#else
            (void)frame;
#endif
        }

        // A frame waiting for its copy to be recorded.
        struct PendingExternalFrame
        {
            campello_gpu::ExternalImage frame;
            float                       uv[6] = {};
            bool                        has_uv = false;
            std::shared_ptr<void>       keep_alive;
            bool                        consumed = false;

            ~PendingExternalFrame() { if (!consumed) releaseUnusedExternalImage(frame); }
        };
    }

    bool RenderVideoPlayer::copyExternalFrame(uint32_t width, uint32_t height,
                                              const campello_gpu::ExternalImage& frame,
                                              const campello_gpu::ExternalImageCopyOptions& options)
    {
        IDrawBackend* backend = RenderObject::activeBackend();
        auto device = backend ? backend->gpuDevice() : nullptr;
        if (width == 0 || height == 0 || !device || !device->supportsExternalImage(frame.type) ||
            !ensureTexture(width, height))
        {
            releaseUnusedExternalImage(frame);   // the copy would have owned it
            return false;
        }
        // The synchronous variant: the frame lands before the next paint
        // samples the texture, and it runs on the backend's own queue.
        return device->copyExternalImageToTexture(frame, video_texture_, options);
    }

    bool RenderVideoPlayer::queueExternalFrame(uint32_t width, uint32_t height,
                                               const campello_gpu::ExternalImage& frame,
                                               const campello_gpu::ExternalImageCopyOptions& options,
                                               std::shared_ptr<void> keep_alive)
    {
        Renderer* renderer = detail::currentRenderer().load(std::memory_order_acquire);
        if (!renderer) return copyExternalFrame(width, height, frame, options);

        IDrawBackend* backend = RenderObject::activeBackend();
        auto device = backend ? backend->gpuDevice() : nullptr;
        if (width == 0 || height == 0 || !device || !device->supportsExternalImage(frame.type) ||
            !ensureTexture(width, height))
        {
            releaseUnusedExternalImage(frame);
            return false;
        }

        auto pending = std::make_shared<PendingExternalFrame>();
        pending->frame      = frame;
        pending->keep_alive = std::move(keep_alive);
        if (options.uvTransform)
        {
            std::copy(options.uvTransform, options.uvTransform + 6, pending->uv);
            pending->has_uv = true;
        }
        renderer->scheduleGpuUpload(
            [pending, texture = video_texture_](campello_gpu::CommandEncoder& encoder)
            {
                campello_gpu::ExternalImageCopyOptions copy_options;
                if (pending->has_uv) copy_options.uvTransform = pending->uv;
                pending->consumed = true;   // the copy owns the fence/handle now
                encoder.copyExternalImageToTexture(pending->frame, texture, copy_options);
                pending->keep_alive.reset();
            });
        return true;
    }

} // namespace systems::leal::campello_widgets
