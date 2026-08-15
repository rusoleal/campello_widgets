#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_gpu/texture.hpp>

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

    void RenderVideoPlayer::uploadFrame(uint32_t width, uint32_t height, const void* bgra_data)
    {
        if (width == 0 || height == 0 || !bgra_data) return;

        IDrawBackend* backend = RenderObject::activeBackend();
        if (!backend) return;

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
            if (!video_texture_) return;
            // Establishes texture_ identity once; further frames reuse the
            // same texture object and call markNeedsPaint() directly (the
            // caller's job — see this method's doc comment), since
            // RenderImage::setTexture() no-ops on an unchanged shared_ptr.
            setTexture(video_texture_);
        }

        const uint64_t length = static_cast<uint64_t>(width) * height * 4;
        video_texture_->upload(0, length, const_cast<void*>(bgra_data));
    }

} // namespace systems::leal::campello_widgets
