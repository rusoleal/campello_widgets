#include <campello_widgets/ui/video_player_controller.hpp>
#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Media Foundation (and the COM apartment it needs) is process-wide
        // and has no natural per-VideoPlayerController lifetime — unlike
        // AVFoundation, which is already running as part of the OS media
        // stack on macOS. Every controller mutation happens on the main UI
        // thread (same assumption VideoPlayerController's header documents
        // for attach()/detach()), so a single lazy, never-torn-down
        // CoInitializeEx()/MFStartup() pair on first use is sufficient —
        // matches this codebase's pattern of not adding teardown machinery
        // for process-lifetime singletons (e.g. TickerScheduler::active()).
        void ensureMediaFoundationInitialized()
        {
            static std::once_flag once;
            std::call_once(once, []() {
                // S_FALSE (already initialized on this thread) and
                // RPC_E_CHANGED_MODE (already initialized with a different
                // concurrency model, e.g. by some other Windows API this
                // process uses) are both fine to proceed with — only a hard
                // FAILED() from CoInitializeEx would mean COM itself is
                // unusable on this thread.
                CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                MFStartup(MF_VERSION);
            });
        }

        uint64_t nowMs()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }
    } // namespace

    struct VideoPlayerController::Impl
    {
        ComPtr<IMFSourceReader> reader;

        uint32_t frame_width  = 0;
        uint32_t frame_height = 0;

        // One-sample lookahead: Media Foundation's synchronous
        // IMFSourceReader::ReadSample() has no way to peek a sample's
        // timestamp without decoding it, so pacing playback to wall-clock
        // time (matching AVFoundation's itemTimeForHostTime/
        // hasNewPixelBufferForItemTime on macOS) requires decoding one
        // frame ahead of when it's due and holding it until its
        // presentation time arrives.
        ComPtr<IMFSample> pending_sample;
        LONGLONG           pending_pts_hns = -1;

        bool at_end = false;

        // Anchors position_ms_/wall-clock together whenever playback starts
        // or a seek lands, so onTick() can compute "how far into the media
        // should we be right now" without a media clock of its own — Source
        // Reader (unlike a full IMFMediaSession) doesn't provide one.
        uint64_t anchor_wall_ms = 0;
        double   anchor_pos_ms  = 0.0;
    };

    VideoPlayerController::VideoPlayerController()
        : impl_(std::make_unique<Impl>())
    {
        ensureMediaFoundationInitialized();
    }

    VideoPlayerController::~VideoPlayerController()
    {
        if (ticker_id_ != 0)
            if (auto* ts = TickerScheduler::active())
                ts->unsubscribe(ticker_id_);
    }

    void VideoPlayerController::setSource(std::string path)
    {
        impl_->reader.Reset();
        impl_->pending_sample.Reset();
        impl_->pending_pts_hns = -1;
        impl_->at_end          = false;
        impl_->frame_width     = 0;
        impl_->frame_height    = 0;

        playing_     = false;
        ready_       = false;
        position_ms_ = 0.0;
        duration_ms_ = 0.0;

        // MFCreateSourceReaderFromURL takes a wide-char path; `path` is a
        // local filesystem path (per this method's contract), so a direct
        // MultiByteToWideChar conversion (not full UTF-8 URL escaping) is
        // sufficient — same "local file path only, streaming out of scope"
        // contract the header documents.
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wlen <= 0) return;
        std::vector<wchar_t> wpath(static_cast<size_t>(wlen));
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

        ComPtr<IMFAttributes> attrs;
        MFCreateAttributes(&attrs, 1);
        // Lets Source Reader insert a decoder + color-converter chain
        // automatically so SetCurrentMediaType() below (requesting
        // MFVideoFormat_RGB32 regardless of the source codec/subtype) just
        // works, the same role AVPlayerItemVideoOutput's pixel-format
        // attributes play on macOS.
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

        ComPtr<IMFSourceReader> reader;
        if (FAILED(MFCreateSourceReaderFromURL(wpath.data(), attrs.Get(), &reader)))
            return;

        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

        ComPtr<IMFMediaType> output_type;
        MFCreateMediaType(&output_type);
        output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        // BGRX8 in memory (byte order B,G,R,X) — matches uploadFrame()'s
        // expected BGRA8 layout exactly except for the unused X/alpha byte,
        // which onTick() below forces to 0xFF (opaque) on copy, the same
        // role the mac backend's "already the offscreen texture's format"
        // comment describes for its own BGRA choice.
        output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(reader->SetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, output_type.Get())))
            return;

        ComPtr<IMFMediaType> current_type;
        if (FAILED(reader->GetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current_type)))
            return;
        UINT32 width = 0, height = 0;
        MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
        if (width == 0 || height == 0) return;

        PROPVARIANT duration_var;
        PropVariantInit(&duration_var);
        double duration_ms = 0.0;
        if (SUCCEEDED(reader->GetPresentationAttribute(
                static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration_var)))
        {
            LONGLONG duration_hns = 0;
            if (SUCCEEDED(PropVariantToInt64(duration_var, &duration_hns)))
                duration_ms = static_cast<double>(duration_hns) / 10000.0;
        }
        PropVariantClear(&duration_var);

        impl_->reader       = reader;
        impl_->frame_width  = width;
        impl_->frame_height = height;
        duration_ms_        = duration_ms;
        // Unlike AVFoundation (which discovers readiness/duration
        // asynchronously and only flips ready_ once AVPlayerItemStatus
        // reports it — see the mac backend's onTick()), Source Reader's
        // SetCurrentMediaType()/GetPresentationAttribute() calls above are
        // already synchronous and blocking, so everything needed to report
        // "ready" is known by the time setSource() returns.
        ready_ = true;

        notifyListeners();
    }

    void VideoPlayerController::play()
    {
        if (!impl_->reader) return;
        playing_             = true;
        impl_->anchor_wall_ms = nowMs();
        impl_->anchor_pos_ms  = position_ms_;
        ensureTicking();
        notifyListeners();
    }

    void VideoPlayerController::pause()
    {
        if (!impl_->reader) return;
        playing_ = false;
        notifyListeners();
    }

    void VideoPlayerController::seekTo(double position_ms)
    {
        if (!impl_->reader) return;
        const double clamped = std::clamp(position_ms, 0.0, duration_ms_);

        PROPVARIANT var;
        InitPropVariantFromInt64(static_cast<LONGLONG>(clamped * 10000.0), &var);
        impl_->reader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);

        impl_->pending_sample.Reset();
        impl_->pending_pts_hns = -1;
        impl_->at_end          = false;

        position_ms_          = clamped;
        impl_->anchor_wall_ms = nowMs();
        impl_->anchor_pos_ms  = clamped;

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
        if (!impl_->reader)
        {
            if (ticker_id_ != 0)
                if (auto* ts = TickerScheduler::active())
                {
                    ts->unsubscribe(ticker_id_);
                    ticker_id_ = 0;
                }
            return;
        }

        if (!(ready_ && playing_ && attached_))
        {
            if (ticker_id_ != 0)
                if (auto* ts = TickerScheduler::active())
                {
                    ts->unsubscribe(ticker_id_);
                    ticker_id_ = 0;
                }
            return;
        }

        const double target_ms =
            impl_->anchor_pos_ms + static_cast<double>(nowMs() - impl_->anchor_wall_ms);

        // Bounded catch-up: decode at most a handful of frames this tick so
        // a long stall (e.g. a debugger pause) can't make onTick() block the
        // UI thread trying to fast-forward through an arbitrarily large
        // backlog of frames all at once.
        for (int i = 0; i < 8 && !impl_->at_end; ++i)
        {
            if (!impl_->pending_sample)
            {
                DWORD stream_flags = 0;
                LONGLONG timestamp = 0;
                ComPtr<IMFSample> sample;
                const HRESULT hr = impl_->reader->ReadSample(
                    static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0,
                    nullptr, &stream_flags, &timestamp, &sample);

                if (FAILED(hr) || (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM))
                {
                    impl_->at_end = true;
                    break;
                }
                if (!sample) break; // Gap in the stream — nothing to show yet.

                impl_->pending_sample  = sample;
                impl_->pending_pts_hns = timestamp;
            }

            const double pending_ms = static_cast<double>(impl_->pending_pts_hns) / 10000.0;
            if (pending_ms > target_ms) break; // Not due yet — hold for a later tick.

            ComPtr<IMFMediaBuffer> buffer;
            if (SUCCEEDED(impl_->pending_sample->ConvertToContiguousBuffer(&buffer)))
            {
                const uint32_t width  = impl_->frame_width;
                const uint32_t height = impl_->frame_height;

                ComPtr<IMF2DBuffer2> buffer_2d;
                BYTE* scanline0 = nullptr;
                LONG  pitch     = 0;
                BYTE* raw_start = nullptr;
                DWORD raw_len   = 0;
                bool  locked_2d = false;

                if (SUCCEEDED(buffer.As(&buffer_2d)) &&
                    SUCCEEDED(buffer_2d->Lock2DSize(MF2DBuffer_LockFlags_Read, &scanline0,
                                                     &pitch, &raw_start, &raw_len)))
                {
                    locked_2d = true;
                }
                else if (SUCCEEDED(buffer->Lock(&scanline0, nullptr, nullptr)))
                {
                    // Buffer doesn't support 2D locking (uncommon for
                    // RGB32 output, but not guaranteed) — fall back to
                    // treating it as tightly packed and top-down.
                    pitch = static_cast<LONG>(width) * 4;
                }
                else
                {
                    scanline0 = nullptr;
                }

                if (scanline0 && width > 0 && height > 0)
                {
                    // Tightly pack rows (pitch may exceed width*4 for
                    // alignment, or be negative for a bottom-up buffer —
                    // scanline0 already points at image row 0 either way,
                    // per IMF2DBuffer2::Lock2DSize's contract, so walking
                    // by signed `pitch` handles both) and force alpha to
                    // opaque, since RGB32's 4th byte is undefined padding,
                    // not a real alpha channel.
                    const size_t tight_row_bytes = static_cast<size_t>(width) * 4;
                    std::vector<uint8_t> packed(tight_row_bytes * height);
                    for (uint32_t y = 0; y < height; ++y)
                    {
                        const uint8_t* src_row =
                            scanline0 + static_cast<ptrdiff_t>(y) * pitch;
                        uint8_t* dst_row = packed.data() + y * tight_row_bytes;
                        std::memcpy(dst_row, src_row, tight_row_bytes);
                        for (uint32_t x = 0; x < width; ++x)
                            dst_row[x * 4 + 3] = 0xFF;
                    }
                    attached_->uploadFrame(width, height, packed.data());
                }

                if (locked_2d)
                    buffer_2d->Unlock2D();
                else
                    buffer->Unlock();
            }

            position_ms_ = pending_ms;
            impl_->pending_sample.Reset();
            impl_->pending_pts_hns = -1;
        }

        if (impl_->at_end)
        {
            onPlaybackEnded();
            return;
        }

        notifyListeners();
        // Keep frames coming while playing — matches
        // RenderSingleChildScrollView::onTick()'s identical "re-arm every
        // tick while active" call.
        FrameScheduler::scheduleFrame();
    }

    void VideoPlayerController::onPlaybackEnded()
    {
        playing_     = false;
        position_ms_ = 0.0;

        if (impl_->reader)
        {
            PROPVARIANT var;
            InitPropVariantFromInt64(0, &var);
            impl_->reader->SetCurrentPosition(GUID_NULL, var);
            PropVariantClear(&var);
        }
        impl_->pending_sample.Reset();
        impl_->pending_pts_hns = -1;
        impl_->at_end          = false;

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
