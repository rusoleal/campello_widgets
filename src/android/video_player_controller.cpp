#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <android/log.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define CW_LOG_TAG "campello_widgets/video"
#define CW_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, CW_LOG_TAG, __VA_ARGS__)

namespace systems::leal::campello_widgets
{

    namespace
    {
        // OMX/MediaCodecInfo.CodecCapabilities color format constants —
        // not exposed as named symbols by the NDK's public headers (only
        // the Java MediaCodecInfo API names them), so declared locally.
        // These are the two concrete layouts actually handled below;
        // anything else (including COLOR_FormatYUV420Flexible, which is a
        // request-time hint rather than a concrete layout some devices
        // still echo back unresolved) falls back to the semi-planar/NV12
        // interpretation, the overwhelmingly common real hardware layout.
        constexpr int32_t kColorFormatYUV420Planar     = 19; // I420: separate Y, U, V
        constexpr int32_t kColorFormatYUV420SemiPlanar = 21; // NV12: Y, then interleaved UV

        // Standard BT.601 YUV -> BGRA conversion (matches the offscreen
        // texture's bgra8unorm format — see IDrawBackend::
        // offscreenPixelFormat() — with no further color-space
        // conversion needed once this runs).
        inline uint8_t clampByte(int v)
        {
            return static_cast<uint8_t>(std::clamp(v, 0, 255));
        }

        // Converts a raw MediaCodec byte-buffer output frame to tightly
        // packed BGRA8. This is the byte-buffer decode path — MediaCodec
        // configured with no output Surface, decoding directly into
        // CPU-mapped buffers via AMediaCodec_getOutputBuffer() — chosen
        // after a Surface + AImageReader approach (the initial
        // implementation) proved unreliable on real hardware: on a
        // Qualcomm/MIUI Android 13 device, AImageReader's CPU-lock path
        // for the decoder's Surface output turned out to expose a
        // UBWC-compressed (tiled) buffer instead of linear YUV — visually
        // confirmed live (structured tile-pattern noise, not decoded
        // video, once read as if it were linear memory) even after
        // requesting AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN. Byte-buffer
        // mode reads directly from the codec's own CPU buffer — no
        // Surface, no gralloc, no tiling — at the cost of needing to
        // handle whichever concrete YUV layout (planar vs semi-planar)
        // the codec actually negotiates, via `color_format`/`stride`/
        // `slice_height` from AMediaCodec_getOutputFormat().
        void convertYuvBufferToBgra(const uint8_t* buf, size_t buf_size, int32_t color_format,
                                      int32_t stride, int32_t slice_height,
                                      uint32_t crop_width, uint32_t crop_height,
                                      std::vector<uint8_t>& out_bgra)
        {
            const uint8_t* y_data = buf;
            const uint8_t* u_data = nullptr;
            const uint8_t* v_data = nullptr;
            int32_t u_row_stride = 0, v_row_stride = 0;
            int32_t u_pixel_stride = 0, v_pixel_stride = 0;

            const size_t y_plane_size = static_cast<size_t>(stride) * slice_height;

            if (color_format == kColorFormatYUV420Planar)
            {
                u_data         = buf + y_plane_size;
                u_row_stride   = stride / 2;
                u_pixel_stride = 1;
                v_data         = u_data + static_cast<size_t>(u_row_stride) * (slice_height / 2);
                v_row_stride   = stride / 2;
                v_pixel_stride = 1;
            }
            else // kColorFormatYUV420SemiPlanar (NV12) — also the fallback default.
            {
                u_data         = buf + y_plane_size; // interleaved U,V,U,V,...
                u_row_stride   = stride;
                u_pixel_stride = 2;
                v_data         = u_data + 1;
                v_row_stride   = stride;
                v_pixel_stride = 2;
            }

            // Sanity check against the actual buffer size before reading
            // — a mismatched/missing stride or slice_height key (see this
            // function's doc comment) would otherwise read out of bounds.
            const size_t needed =
                std::max(y_plane_size,
                          static_cast<size_t>(v_data - buf) +
                              static_cast<size_t>(v_row_stride) * (slice_height / 2));
            if (needed > buf_size)
            {
                CW_LOGE("buffer too small: needed=%zu have=%zu stride=%d slice_height=%d",
                         needed, buf_size, stride, slice_height);
                return;
            }

            out_bgra.resize(static_cast<size_t>(crop_width) * crop_height * 4);

            for (uint32_t row = 0; row < crop_height; ++row)
            {
                const uint8_t* y_row = y_data + static_cast<size_t>(row) * stride;
                const uint8_t* u_row = u_data + static_cast<size_t>(row / 2) * u_row_stride;
                const uint8_t* v_row = v_data + static_cast<size_t>(row / 2) * v_row_stride;
                uint8_t* out_row     = out_bgra.data() + static_cast<size_t>(row) * crop_width * 4;

                for (uint32_t col = 0; col < crop_width; ++col)
                {
                    const int y_val = y_row[col];
                    const int u_val = u_row[(col / 2) * u_pixel_stride] - 128;
                    const int v_val = v_row[(col / 2) * v_pixel_stride] - 128;

                    const int r = y_val + (1402 * v_val) / 1000;
                    const int g = y_val - (344136 * u_val) / 1000000 - (714136 * v_val) / 1000000;
                    const int b = y_val + (1772 * u_val) / 1000;

                    uint8_t* px = out_row + static_cast<size_t>(col) * 4;
                    px[0] = clampByte(b);
                    px[1] = clampByte(g);
                    px[2] = clampByte(r);
                    px[3] = 255;
                }
            }
        }

        // Holds the actual native decode state. Deliberately *not* nested
        // inside VideoPlayerController::Impl: the free-function decode
        // thread (decodeLoop(), below) needs to touch these fields, and a
        // private nested class is only accessible to VideoPlayerController's
        // own member functions, not to arbitrary functions in this
        // translation unit. VideoPlayerController::Impl (which the shared,
        // cross-platform header does declare as private) is kept as a thin
        // wrapper around a pointer to this instead.
        struct AndroidVideoState
        {
            AMediaExtractor* extractor = nullptr;
            AMediaCodec*     codec     = nullptr;

            // The video's logical display dimensions (from the container
            // track format) — set once in setSource(), read-only from
            // decodeLoop() afterward, safe without synchronization since
            // the decode thread is only started after these are set, and
            // a later setSource() call joins that thread before touching
            // this state again.
            int32_t crop_width  = 0;
            int32_t crop_height = 0;

            std::thread             decode_thread;
            std::mutex               mutex;
            std::condition_variable  cv;
            bool                     playing_flag   = false;  // decode thread's own view of play/pause
            bool                     stop_requested = false;

            // Latest decoded frame, handed off from the decode thread to
            // onTick() (main thread). Mirrors ImageWidgetState::
            // checkFuture()'s async-work/main-thread-poll split, but
            // continuous.
            std::vector<uint8_t> latest_bgra;
            uint32_t             latest_width       = 0;
            uint32_t             latest_height      = 0;
            double               latest_position_ms = 0.0;
            uint64_t             generation          = 0;
            uint64_t             uploaded_generation = 0;

            std::atomic<bool> end_of_stream{false};
        };

        void decodeLoop(AndroidVideoState* state)
        {
            using clock = std::chrono::steady_clock;

            bool input_eos  = false;
            bool output_eos = false;
            std::optional<clock::time_point> playback_start_wall;
            std::optional<int64_t>           playback_start_pts_us;

            // Defaults match a tightly-packed buffer at the video's own
            // dimensions — used only if AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED
            // never fires (it always should, once, before real output
            // arrives, but this avoids reading through zero-initialized
            // stride/slice_height in that unexpected case).
            int32_t color_format = kColorFormatYUV420SemiPlanar;
            int32_t out_stride       = state->crop_width;
            int32_t out_slice_height = state->crop_height;

            while (true)
            {
                {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->cv.wait(lock, [&] {
                        return state->stop_requested || state->playing_flag;
                    });
                    if (state->stop_requested) return;
                }

                if (!input_eos)
                {
                    const ssize_t buf_idx = AMediaCodec_dequeueInputBuffer(state->codec, 2000);
                    if (buf_idx >= 0)
                    {
                        size_t buf_size = 0;
                        uint8_t* buf = AMediaCodec_getInputBuffer(state->codec, buf_idx, &buf_size);
                        const ssize_t sample_size =
                            buf ? AMediaExtractor_readSampleData(state->extractor, buf, buf_size) : -1;

                        if (sample_size < 0)
                        {
                            AMediaCodec_queueInputBuffer(state->codec, buf_idx, 0, 0, 0,
                                                          AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                            input_eos = true;
                        }
                        else
                        {
                            const int64_t pts = AMediaExtractor_getSampleTime(state->extractor);
                            AMediaCodec_queueInputBuffer(state->codec, buf_idx, 0,
                                                          static_cast<size_t>(sample_size), pts, 0);
                            AMediaExtractor_advance(state->extractor);
                        }
                    }
                }

                AMediaCodecBufferInfo info;
                const ssize_t out_idx =
                    AMediaCodec_dequeueOutputBuffer(state->codec, &info, 2000);

                if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
                {
                    AMediaFormat* out_format = AMediaCodec_getOutputFormat(state->codec);
                    if (out_format)
                    {
                        AMediaFormat_getInt32(out_format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color_format);
                        if (!AMediaFormat_getInt32(out_format, AMEDIAFORMAT_KEY_STRIDE, &out_stride))
                            out_stride = state->crop_width;
                        if (!AMediaFormat_getInt32(out_format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &out_slice_height))
                            out_slice_height = state->crop_height;
                        AMediaFormat_delete(out_format);
                    }
                }
                else if (out_idx >= 0)
                {
                    // Pace playback against wall-clock time using each
                    // frame's presentation timestamp — no audio clock to
                    // sync to in this video-only slice.
                    const auto now = clock::now();
                    if (!playback_start_wall)
                    {
                        playback_start_wall   = now;
                        playback_start_pts_us = info.presentationTimeUs;
                    }
                    const int64_t elapsed_pts_us = info.presentationTimeUs - *playback_start_pts_us;
                    const auto target_wall =
                        *playback_start_wall + std::chrono::microseconds(elapsed_pts_us);
                    if (target_wall > now)
                        std::this_thread::sleep_until(target_wall);

                    if (info.size > 0)
                    {
                        size_t buf_size = 0;
                        uint8_t* buf = AMediaCodec_getOutputBuffer(state->codec, out_idx, &buf_size);
                        if (buf)
                        {
                            std::vector<uint8_t> bgra;
                            convertYuvBufferToBgra(buf + info.offset, buf_size - info.offset,
                                                     color_format, out_stride, out_slice_height,
                                                     static_cast<uint32_t>(state->crop_width),
                                                     static_cast<uint32_t>(state->crop_height),
                                                     bgra);
                            if (!bgra.empty())
                            {
                                std::lock_guard<std::mutex> lock(state->mutex);
                                state->latest_bgra        = std::move(bgra);
                                state->latest_width       = static_cast<uint32_t>(state->crop_width);
                                state->latest_height      = static_cast<uint32_t>(state->crop_height);
                                state->latest_position_ms = info.presentationTimeUs / 1000.0;
                                state->generation++;
                            }
                        }
                    }

                    // No output Surface in this decode mode — nothing to
                    // render to, just return the buffer to the codec.
                    AMediaCodec_releaseOutputBuffer(state->codec, out_idx, false);

                    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                    {
                        output_eos = true;
                    }
                }

                if (output_eos)
                {
                    state->end_of_stream = true;
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->playing_flag = false;
                    // Wait to be reset (seekTo()/setSource() restart the
                    // loop) rather than spinning once the stream is done.
                    state->cv.wait(lock, [&] {
                        return state->stop_requested || state->playing_flag;
                    });
                    if (state->stop_requested) return;
                    input_eos  = false;
                    output_eos = false;
                    playback_start_wall.reset();
                    playback_start_pts_us.reset();
                }
            }
        }
    } // namespace

    struct VideoPlayerController::Impl
    {
        std::unique_ptr<AndroidVideoState> state = std::make_unique<AndroidVideoState>();
    };

    VideoPlayerController::VideoPlayerController()
        : impl_(std::make_unique<Impl>())
    {
    }

    VideoPlayerController::~VideoPlayerController()
    {
        if (ticker_id_ != 0)
            if (auto* ts = TickerScheduler::active())
                ts->unsubscribe(ticker_id_);

        AndroidVideoState* state = impl_->state.get();

        if (state->decode_thread.joinable())
        {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->stop_requested = true;
            }
            state->cv.notify_all();
            state->decode_thread.join();
        }

        if (state->codec)
        {
            AMediaCodec_stop(state->codec);
            AMediaCodec_delete(state->codec);
        }
        if (state->extractor) AMediaExtractor_delete(state->extractor);
    }

    void VideoPlayerController::setSource(std::string path)
    {
        AndroidVideoState* state = impl_->state.get();

        // Tear down any previous source's native state before starting a
        // new one — mirrors AVFoundation's replaceCurrentItemWithPlayerItem
        // path, just without an equivalent "swap in place" primitive here.
        if (state->decode_thread.joinable())
        {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->stop_requested = true;
            }
            state->cv.notify_all();
            state->decode_thread.join();
            state->stop_requested = false;
        }
        if (state->codec) { AMediaCodec_stop(state->codec); AMediaCodec_delete(state->codec); state->codec = nullptr; }
        if (state->extractor) { AMediaExtractor_delete(state->extractor); state->extractor = nullptr; }

        playing_     = false;
        ready_       = false;
        position_ms_ = 0.0;
        duration_ms_ = 0.0;
        state->end_of_stream = false;
        state->generation = state->uploaded_generation = 0;

        state->extractor = AMediaExtractor_new();
        // AMediaExtractor_setDataSource() takes a URI (its own doc
        // comment says so) and is really meant for network sources —
        // both a bare absolute path and a "file://" URI failed here on
        // device even though the file demonstrably exists and is
        // readable (found by testing: both compile and link fine, both
        // fail at runtime with no other symptom). setDataSourceFd() is
        // the documented, reliable way to open a local file — same
        // fd-ownership contract as the Java MediaExtractor.setDataSource
        // (FileDescriptor) it mirrors: the extractor dups the fd
        // internally, so it's safe to close ours right after the call.
        const int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            CW_LOGE("open() failed for %s", path.c_str());
            AMediaExtractor_delete(state->extractor);
            state->extractor = nullptr;
            notifyListeners();
            return;
        }
        struct stat st{};
        fstat(fd, &st);
        const media_status_t src_status =
            AMediaExtractor_setDataSourceFd(state->extractor, fd, 0, st.st_size);
        close(fd);
        if (src_status != AMEDIA_OK)
        {
            CW_LOGE("AMediaExtractor_setDataSourceFd failed for %s", path.c_str());
            AMediaExtractor_delete(state->extractor);
            state->extractor = nullptr;
            notifyListeners();
            return;
        }

        const size_t track_count = AMediaExtractor_getTrackCount(state->extractor);
        int video_track = -1;
        AMediaFormat* format = nullptr;
        for (size_t i = 0; i < track_count; ++i)
        {
            AMediaFormat* f = AMediaExtractor_getTrackFormat(state->extractor, i);
            const char* mime = nullptr;
            if (AMediaFormat_getString(f, AMEDIAFORMAT_KEY_MIME, &mime) && mime &&
                std::strncmp(mime, "video/", 6) == 0)
            {
                video_track = static_cast<int>(i);
                format = f;
                break;
            }
            AMediaFormat_delete(f);
        }

        if (video_track < 0 || !format)
        {
            CW_LOGE("No video track found in %s", path.c_str());
            AMediaExtractor_delete(state->extractor);
            state->extractor = nullptr;
            notifyListeners();
            return;
        }

        AMediaExtractor_selectTrack(state->extractor, static_cast<size_t>(video_track));

        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &state->crop_width);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &state->crop_height);

        int64_t duration_us = 0;
        if (AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &duration_us))
            duration_ms_ = duration_us / 1000.0;

        const char* mime = nullptr;
        AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
        state->codec = AMediaCodec_createDecoderByType(mime);
        // No output Surface (nullptr) — byte-buffer decode mode. See
        // convertYuvBufferToBgra()'s doc comment for why: a Surface +
        // AImageReader CPU-read path turned out to hand back a
        // UBWC-compressed buffer on real Qualcomm hardware, not linear
        // YUV, even when explicitly requesting CPU-read usage.
        AMediaCodec_configure(state->codec, format, nullptr, nullptr, 0);
        AMediaCodec_start(state->codec);
        AMediaFormat_delete(format);

        ready_ = true;

        state->decode_thread = std::thread(decodeLoop, state);

        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::play()
    {
        if (!ready_) return;
        AndroidVideoState* state = impl_->state.get();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->playing_flag = true;
        }
        state->cv.notify_all();
        playing_ = true;
        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::pause()
    {
        AndroidVideoState* state = impl_->state.get();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->playing_flag = false;
        }
        state->cv.notify_all();
        playing_ = false;
        notifyListeners();
    }

    void VideoPlayerController::seekTo(double position_ms)
    {
        AndroidVideoState* state = impl_->state.get();
        if (!ready_ || !state->extractor) return;
        const double clamped = std::clamp(position_ms, 0.0, duration_ms_);

        // Pause the decode thread while we touch the extractor/codec out
        // from under it.
        bool was_playing = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            was_playing = state->playing_flag;
            state->playing_flag = false;
        }
        state->cv.notify_all();

        AMediaExtractor_seekTo(state->extractor, static_cast<int64_t>(clamped * 1000.0),
                               AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
        AMediaCodec_flush(state->codec);
        state->end_of_stream = false;

        position_ms_ = clamped;

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->playing_flag = was_playing;
        }
        if (was_playing) state->cv.notify_all();

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
        if (!ready_)
            return;

        AndroidVideoState* state = impl_->state.get();

        if (state->end_of_stream.load())
        {
            onPlaybackEnded();
            return;
        }

        if (playing_ && attached_)
        {
            uint64_t gen = 0;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                gen = state->generation;
            }

            if (gen != state->uploaded_generation)
            {
                std::vector<uint8_t> frame;
                uint32_t width = 0, height = 0;
                double position_ms = position_ms_;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    frame        = state->latest_bgra;
                    width        = state->latest_width;
                    height       = state->latest_height;
                    position_ms  = state->latest_position_ms;
                }

                if (width > 0 && height > 0 && !frame.empty())
                {
                    attached_->uploadFrame(width, height, frame.data());
                    attached_->markNeedsPaint();
                }
                state->uploaded_generation = gen;
                position_ms_ = position_ms;
                notifyListeners();
            }

            // Keep frames coming while playing — matches the AVFoundation
            // backend's identical "re-arm every tick while active" call.
            FrameScheduler::scheduleFrame();
        }
        else if (!playing_)
        {
            if (auto* ts = TickerScheduler::active())
            {
                ts->unsubscribe(ticker_id_);
                ticker_id_ = 0;
            }
        }
    }

    void VideoPlayerController::onPlaybackEnded()
    {
        AndroidVideoState* state = impl_->state.get();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->playing_flag = false;
        }
        state->cv.notify_all();

        if (state->extractor)
        {
            AMediaExtractor_seekTo(state->extractor, 0, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
            if (state->codec) AMediaCodec_flush(state->codec);
        }
        state->end_of_stream = false;

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
