// VideoPlayerController on Linux: not implemented yet (GStreamer or FFmpeg
// decoding into a texture would be the natural backend). A source never
// becomes ready, so VideoPlayer widgets show their placeholder. Without this
// stub, apps using VideoPlayer (e.g. the gallery) didn't link on Linux.

#include <campello_widgets/ui/video_player_controller.hpp>

#include <cstdio>

namespace systems::leal::campello_widgets
{

    struct VideoPlayerController::Impl {};

    VideoPlayerController::VideoPlayerController() = default;
    VideoPlayerController::~VideoPlayerController() = default;

    void VideoPlayerController::setSource(std::string path)
    {
        std::fprintf(stderr, "[Linux] VideoPlayerController: video is not supported yet (%s)\n",
                     path.c_str());
        ready_ = false;
        notifyListeners();
    }

    void VideoPlayerController::play() {}
    void VideoPlayerController::pause() {}
    void VideoPlayerController::seekTo(double) {}

    uint64_t VideoPlayerController::addListener(std::function<void()> fn)
    {
        const uint64_t id = next_listener_id_++;
        listeners_.emplace_back(id, std::move(fn));
        return id;
    }

    void VideoPlayerController::removeListener(uint64_t id)
    {
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
            if (it->first == id) {
                listeners_.erase(it);
                return;
            }
        }
    }

    void VideoPlayerController::notifyListeners()
    {
        auto copy = listeners_;
        for (auto& [id, fn] : copy) {
            if (fn) fn();
        }
    }

    void VideoPlayerController::ensureTicking() {}
    void VideoPlayerController::onTick(uint64_t) {}
    void VideoPlayerController::onPlaybackEnded() {}

} // namespace systems::leal::campello_widgets
