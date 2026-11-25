#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

namespace systems::leal::campello_widgets
{

    struct VideoPlayerController::Impl
    {
        AVPlayer                *player       = nil;
        AVPlayerItemVideoOutput *video_output = nil;
        // Token for the AVPlayerItemDidPlayToEndTimeNotification observer
        // — see onPlaybackEnded()'s doc comment for why this exists at
        // all. Must be removed before `this` (captured raw by the
        // observer's block) becomes invalid, i.e. no later than the
        // destructor.
        id<NSObject>              end_observer = nil;
        // Frames go to the GPU as NV12 CVPixelBuffers through
        // RenderVideoPlayer::copyExternalFrame(), with no CPU copy. Cleared
        // (and the output swapped for a BGRA one feeding uploadFrame()) the
        // first time that copy fails.
        bool                      zero_copy    = true;
    };

    namespace
    {
        // NV12 ('420v', what the decoders produce, so AVFoundation doesn't
        // convert) and Metal-compatible for the zero-copy path; otherwise
        // BGRA so the CPU copy matches the offscreen texture's format
        // (bgra8unorm — see IDrawBackend::offscreenPixelFormat()) with no
        // color-space conversion.
        AVPlayerItemVideoOutput* makeVideoOutput(bool zero_copy)
        {
            NSDictionary* attrs = zero_copy
                ? @{
                      (NSString*)kCVPixelBufferPixelFormatTypeKey :
                          @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
                      (NSString*)kCVPixelBufferMetalCompatibilityKey : @YES,
                  }
                : @{
                      (NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
                  };
            return [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attrs];
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
        if (impl_->end_observer)
            [[NSNotificationCenter defaultCenter] removeObserver:impl_->end_observer];
        [impl_->player pause];
    }

    void VideoPlayerController::setSource(std::string path)
    {
        @autoreleasepool
        {
            NSString* ns_path = [NSString stringWithUTF8String:path.c_str()];
            NSURL*    url     = [NSURL fileURLWithPath:ns_path];

            AVURLAsset*   asset = [AVURLAsset URLAssetWithURL:url options:nil];
            AVPlayerItem* item  = [AVPlayerItem playerItemWithAsset:asset];

            // No backend yet (setSource() before the first frame) means
            // the Metal one is coming, which can import CVPixelBuffers.
            impl_->zero_copy =
                !RenderObject::activeBackend() ||
                RenderVideoPlayer::supportsExternalFrames(
                    campello_gpu::ExternalImageType::cvPixelBuffer);
            impl_->video_output = makeVideoOutput(impl_->zero_copy);
            [item addOutput:impl_->video_output];

            if (impl_->player)
                [impl_->player replaceCurrentItemWithPlayerItem:item];
            else
                impl_->player = [AVPlayer playerWithPlayerItem:item];

            if (impl_->end_observer)
            {
                [[NSNotificationCenter defaultCenter] removeObserver:impl_->end_observer];
                impl_->end_observer = nil;
            }
            // Block-based rather than a dedicated Objective-C delegate
            // class — captures `this` directly (safe: removed in the
            // destructor before `this` can become invalid, and this block
            // runs only on the main queue, the same thread every other
            // mutation of this controller's state happens on). See
            // onPlaybackEnded()'s doc comment for why this notification
            // (rather than polling) is what's needed here.
            impl_->end_observer = [[NSNotificationCenter defaultCenter]
                addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
                            object:item
                             queue:[NSOperationQueue mainQueue]
                        usingBlock:^(NSNotification* _Nonnull) {
                            this->onPlaybackEnded();
                        }];

            playing_     = false;
            ready_       = false;
            position_ms_ = 0.0;
            duration_ms_ = 0.0;

            // Starts polling for AVPlayerItemStatusReadyToPlay — see
            // onTick()'s doc comment. Not KVO/notification-driven,
            // deliberately: matches this codebase's established pattern
            // (ImageWidgetState::checkFuture()) of polling on a main-thread
            // ticker rather than bridging an async Objective-C callback
            // into C++.
            ensureTicking();
            notifyListeners();
        }
    }

    void VideoPlayerController::play()
    {
        if (!impl_->player) return;
        [impl_->player play];
        playing_ = true;
        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::pause()
    {
        if (!impl_->player) return;
        [impl_->player pause];
        playing_ = false;
        notifyListeners();
    }

    void VideoPlayerController::seekTo(double position_ms)
    {
        if (!impl_->player) return;
        const double clamped = std::clamp(position_ms, 0.0, duration_ms_);
        const CMTime target  = CMTimeMakeWithSeconds(clamped / 1000.0, NSEC_PER_SEC);
        [impl_->player seekToTime:target toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
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
        if (!impl_->player)
        {
            if (ticker_id_ != 0)
                if (auto* ts = TickerScheduler::active())
                {
                    ts->unsubscribe(ticker_id_);
                    ticker_id_ = 0;
                }
            return;
        }

        AVPlayerItem* item = impl_->player.currentItem;

        if (!ready_)
        {
            if (item.status == AVPlayerItemStatusReadyToPlay)
            {
                const double dur = CMTimeGetSeconds(item.duration);
                duration_ms_     = std::isfinite(dur) ? dur * 1000.0 : 0.0;
                ready_           = true;
                notifyListeners();
            }
            else if (item.status == AVPlayerItemStatusFailed)
            {
                // Nothing more this source will ever produce — stop
                // polling it. Left !ready_/playing_ = false; no separate
                // error-state field in this first slice (see TODO.md).
                if (auto* ts = TickerScheduler::active())
                {
                    ts->unsubscribe(ticker_id_);
                    ticker_id_ = 0;
                }
                return;
            }
        }

        if (ready_ && playing_ && attached_)
        {
            const CMTime item_time =
                [impl_->video_output itemTimeForHostTime:CACurrentMediaTime()];

            if ([impl_->video_output hasNewPixelBufferForItemTime:item_time])
            {
                CVPixelBufferRef pixel_buffer =
                    [impl_->video_output copyPixelBufferForItemTime:item_time
                                                   itemTimeForDisplay:NULL];
                if (pixel_buffer && impl_->zero_copy)
                {
                    campello_gpu::ExternalImage frame;
                    frame.type   = campello_gpu::ExternalImageType::cvPixelBuffer;
                    frame.handle = pixel_buffer;
                    const uint32_t width  = static_cast<uint32_t>(CVPixelBufferGetWidth(pixel_buffer));
                    const uint32_t height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixel_buffer));
                    // Recorded into the next frame. The retain covers the
                    // wait for that; campello_gpu then retains the buffer
                    // until the GPU is done with it.
                    std::shared_ptr<void> keep_alive(CVPixelBufferRetain(pixel_buffer),
                                                     [](void* p) { CVPixelBufferRelease(static_cast<CVPixelBufferRef>(p)); });
                    if (!attached_->queueExternalFrame(width, height, frame, {}, std::move(keep_alive)))
                    {
                        // Shows nothing this frame; BGRA ones from the next.
                        impl_->zero_copy = false;
                        AVPlayerItemVideoOutput* bgra = makeVideoOutput(false);
                        [item removeOutput:impl_->video_output];
                        [item addOutput:bgra];
                        impl_->video_output = bgra;
                    }
                    CVPixelBufferRelease(pixel_buffer);
                }
                else if (pixel_buffer)
                {
                    CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);

                    const uint32_t width       = static_cast<uint32_t>(CVPixelBufferGetWidth(pixel_buffer));
                    const uint32_t height      = static_cast<uint32_t>(CVPixelBufferGetHeight(pixel_buffer));
                    const size_t   bytes_per_row = CVPixelBufferGetBytesPerRow(pixel_buffer);
                    const uint8_t* base        = static_cast<const uint8_t*>(
                        CVPixelBufferGetBaseAddress(pixel_buffer));
                    const size_t   tight_row_bytes = static_cast<size_t>(width) * 4;

                    if (base && width > 0 && height > 0)
                    {
                        if (bytes_per_row == tight_row_bytes)
                        {
                            // Already tightly packed — upload directly.
                            attached_->uploadFrame(width, height, base);
                        }
                        else
                        {
                            // RenderVideoPlayer::uploadFrame() (and
                            // campello_gpu::Texture::upload() beneath it)
                            // expects tightly-packed rows with no
                            // per-row alignment padding, but
                            // CVPixelBufferGetBytesPerRow() is free to
                            // (and often does) pad each row wider than
                            // width * 4 for alignment — copy row by row
                            // into a tightly-packed scratch buffer first.
                            std::vector<uint8_t> packed(tight_row_bytes * height);
                            for (uint32_t y = 0; y < height; ++y)
                            {
                                std::memcpy(packed.data() + y * tight_row_bytes,
                                            base + y * bytes_per_row,
                                            tight_row_bytes);
                            }
                            attached_->uploadFrame(width, height, packed.data());
                        }
                    }

                    CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
                    CVPixelBufferRelease(pixel_buffer);
                }
            }

            const double pos = CMTimeGetSeconds(impl_->player.currentTime);
            position_ms_      = std::isfinite(pos) ? pos * 1000.0 : position_ms_;
            notifyListeners();

            // Keep frames coming while playing — matches
            // RenderSingleChildScrollView::onTick()'s identical
            // "re-arm every tick while active" call.
            FrameScheduler::scheduleFrame();
        }
        else if (ready_ && !playing_)
        {
            // Nothing left to poll for until play() or a new setSource()
            // — see ensureTicking()'s doc comment for why both of those
            // re-subscribe.
            if (auto* ts = TickerScheduler::active())
            {
                ts->unsubscribe(ticker_id_);
                ticker_id_ = 0;
            }
        }
    }

    void VideoPlayerController::onPlaybackEnded()
    {
        if (impl_->player)
        {
            [impl_->player pause];
            [impl_->player seekToTime:kCMTimeZero
                       toleranceBefore:kCMTimeZero
                        toleranceAfter:kCMTimeZero];
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
        auto it = std::find_if(listeners_.begin(), listeners_.end(),
            [id](const auto& p) { return p.first == id; });
        if (it != listeners_.end())
            listeners_.erase(it);
    }

    void VideoPlayerController::notifyListeners()
    {
        for (auto& [id, fn] : listeners_)
        {
            (void)id;
            fn();
        }
    }

} // namespace systems::leal::campello_widgets
