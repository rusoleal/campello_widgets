#include <campello_widgets/widgets/video_player.hpp>
#include <campello_widgets/ui/render_video_player.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> VideoPlayer::createRenderObject() const
    {
        auto ro = std::make_shared<RenderVideoPlayer>(controller);
        ro->setFit(fit);
        ro->setAlignment(alignment);
        return ro;
    }

    void VideoPlayer::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro = static_cast<RenderVideoPlayer&>(render_object);
        ro.setController(controller);
        ro.setFit(fit);
        ro.setAlignment(alignment);
    }

} // namespace systems::leal::campello_widgets
