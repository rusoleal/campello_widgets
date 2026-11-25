// VideoPlayerController on the web: the browser decodes (a hidden <video>,
// see cw_web_video_* in campello_widgets_web.js) and each new frame goes to
// the GPU as a campello_gpu webImageSource (GPUQueue.copyExternalImageToTexture
// under RenderVideoPlayer::copyExternalFrame()), so frames never pass
// through wasm memory. `path` is a URL, relative to the page or absolute
// (a cross-origin one must allow CORS). Polled from a TickerScheduler
// subscription, like the AVFoundation backend.

#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>

extern "C"
{
    int  cw_web_video_open(const char* url);
    void cw_web_video_state(int id, double* out);
    void cw_web_video_play(int id);
    void cw_web_video_pause(int id);
    void cw_web_video_seek(int id, double ms);
    void cw_web_video_close(int id);
    int  cw_web_video_import(int id);
}

namespace systems::leal::campello_widgets
{

    struct VideoPlayerController::Impl
    {
        int    id              = 0;
        double frames_shown    = -1.0;   // presented-frame count last copied
        bool   reported_failed = false;
    };

    namespace
    {
        struct WebVideoState
        {
            bool     ready = false;
            double   duration_ms = 0, position_ms = 0, frames = 0;
            uint32_t width = 0, height = 0;
            bool     ended = false, failed = false;
        };

        WebVideoState stateOf(int id)
        {
            double v[8] = {};
            cw_web_video_state(id, v);
            WebVideoState s;
            s.ready       = v[0] != 0;
            s.duration_ms = v[1];
            s.position_ms = v[2];
            s.frames      = v[3];
            s.width       = static_cast<uint32_t>(v[4]);
            s.height      = static_cast<uint32_t>(v[5]);
            s.ended       = v[6] != 0;
            s.failed      = v[7] != 0;
            return s;
        }
    }

    VideoPlayerController::VideoPlayerController()
        : impl_(std::make_unique<Impl>())
    {
    }

    VideoPlayerController::~VideoPlayerController()
    {
        if (ticker_id_ != 0)
            if (auto* ts = TickerScheduler::active())
                ts->unsubscribe(ticker_id_);
        if (impl_->id) cw_web_video_close(impl_->id);
    }

    void VideoPlayerController::setSource(std::string path)
    {
        if (impl_->id) cw_web_video_close(impl_->id);
        impl_->id              = cw_web_video_open(path.c_str());
        impl_->frames_shown    = -1.0;
        impl_->reported_failed = false;

        playing_     = false;
        ready_       = false;
        position_ms_ = 0.0;
        duration_ms_ = 0.0;

        // Polls for readiness (HAVE_CURRENT_DATA), as the AVFoundation
        // backend polls AVPlayerItemStatusReadyToPlay.
        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::play()
    {
        if (!impl_->id) return;
        cw_web_video_play(impl_->id);
        playing_ = true;
        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::pause()
    {
        if (!impl_->id) return;
        cw_web_video_pause(impl_->id);
        playing_ = false;
        notifyListeners();
    }

    void VideoPlayerController::seekTo(double position_ms)
    {
        if (!impl_->id) return;
        const double clamped = std::clamp(position_ms, 0.0, duration_ms_);
        cw_web_video_seek(impl_->id, clamped);
        position_ms_ = clamped;
        notifyListeners();
    }

    void VideoPlayerController::ensureTicking()
    {
        if (ticker_id_ != 0) return;
        auto* ts = TickerScheduler::active();
        if (!ts) return;
        ticker_id_ = ts->subscribe([this](uint64_t now_ms) { onTick(now_ms); });
    }

    void VideoPlayerController::onTick(uint64_t /*now_ms*/)
    {
        auto stopTicking = [this] {
            if (ticker_id_ == 0) return;
            if (auto* ts = TickerScheduler::active()) ts->unsubscribe(ticker_id_);
            ticker_id_ = 0;
        };
        if (!impl_->id) { stopTicking(); return; }

        const WebVideoState s = stateOf(impl_->id);
        if (s.failed)
        {
            // Nothing more this source will produce (bad URL, unsupported
            // codec, CORS); left not ready, as on the other platforms.
            if (!impl_->reported_failed)
                std::fprintf(stderr, "[Web] VideoPlayerController: the video failed to load\n");
            impl_->reported_failed = true;
            stopTicking();
            return;
        }

        if (!ready_ && s.ready)
        {
            duration_ms_ = s.duration_ms;
            ready_       = true;
            notifyListeners();
        }
        if (s.ended)
        {
            onPlaybackEnded();
            return;
        }

        if (ready_ && playing_ && attached_)
        {
            if (s.frames != impl_->frames_shown && s.width > 0 && s.height > 0)
            {
                if (const int handle = cw_web_video_import(impl_->id))
                {
                    campello_gpu::ExternalImage frame;
                    frame.type   = campello_gpu::ExternalImageType::webImageSource;
                    frame.handle = reinterpret_cast<void*>(static_cast<intptr_t>(handle));
                    // Recorded into the next frame; uses up the handle,
                    // copied or not.
                    if (attached_->queueExternalFrame(s.width, s.height, frame))
                    {
                        attached_->markNeedsPaint();
                        impl_->frames_shown = s.frames;
                    }
                }
            }
            position_ms_ = s.position_ms;
            notifyListeners();
            FrameScheduler::scheduleFrame();
        }
        else if (ready_ && !playing_)
        {
            stopTicking();
        }
    }

    void VideoPlayerController::onPlaybackEnded()
    {
        if (impl_->id)
        {
            cw_web_video_pause(impl_->id);
            cw_web_video_seek(impl_->id, 0.0);
        }
        playing_     = false;
        position_ms_ = 0.0;
        if (ticker_id_ != 0)
            if (auto* ts = TickerScheduler::active())
            {
                ts->unsubscribe(ticker_id_);
                ticker_id_ = 0;
            }
        notifyListeners();
    }

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

} // namespace systems::leal::campello_widgets
