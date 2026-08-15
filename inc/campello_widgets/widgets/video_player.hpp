#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    class VideoPlayerController;

    /**
     * @brief Displays the video a `VideoPlayerController` is playing.
     *
     * A plain `RenderObjectWidget`, not `StatefulWidget` — there's no
     * Element-tree state to own here (same reasoning as `DrawSurface`):
     * playback state lives entirely in the externally-owned `controller`,
     * and `RenderVideoPlayer` updates its own texture/repaints itself
     * directly in response to the controller's ticks, with no rebuild
     * needed for the common per-frame case.
     *
     * @code
     * auto ctrl = std::make_shared<VideoPlayerController>();
     * ctrl->setSource("/path/to/video.mp4");
     * auto player = VideoPlayer::create(ctrl);
     * ctrl->play();
     * @endcode
     */
    class VideoPlayer : public RenderObjectWidget
    {
    public:
        std::shared_ptr<VideoPlayerController> controller;
        BoxFit    fit       = BoxFit::contain;
        Alignment alignment = Alignment::center();

        static std::shared_ptr<VideoPlayer> create(
            std::shared_ptr<VideoPlayerController> controller,
            BoxFit                                 fit       = BoxFit::contain,
            Alignment                               alignment = Alignment::center())
        {
            auto w = std::make_shared<VideoPlayer>();
            w->controller = std::move(controller);
            w->fit        = fit;
            w->alignment  = alignment;
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
