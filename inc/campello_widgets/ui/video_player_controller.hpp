#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace systems::leal::campello_widgets
{

    class RenderVideoPlayer;

    /**
     * @brief Controls playback of a video shown by a `VideoPlayer` widget.
     *
     * Created and owned by application code, independently of any widget —
     * mirrors `ScrollController`'s detached-controller shape. A
     * `VideoPlayer`'s `RenderVideoPlayer` calls `attach()`/`detach()` when it
     * mounts/unmounts (see those methods' doc comments for why this one
     * holds a pointer back, unlike `ScrollController`).
     *
     * The actual decode/playback implementation is platform-specific and
     * lives entirely outside this header (macOS: `src/macos/
     * video_player_controller.mm`, via AVFoundation) — this header declares
     * only the platform-neutral surface, the same split `HttpClient` uses.
     * Only macOS has an implementation right now; linking this on another
     * platform will fail until that platform gets its own `.cpp`/`.mm`.
     *
     * @code
     * auto ctrl = std::make_shared<VideoPlayerController>();
     * ctrl->setSource("/path/to/video.mp4");
     * ctrl->play();
     * // ... pass ctrl to a VideoPlayer widget ...
     * @endcode
     */
    class VideoPlayerController
    {
    public:
        VideoPlayerController();
        ~VideoPlayerController();

        // Non-copyable — owns native player state.
        VideoPlayerController(const VideoPlayerController&)            = delete;
        VideoPlayerController& operator=(const VideoPlayerController&) = delete;

        // ------------------------------------------------------------------
        // Source / lifecycle
        // ------------------------------------------------------------------

        /**
         * @brief Loads a video from a local file path.
         *
         * Playback state resets (paused, position 0) until the asset finishes
         * loading, at which point `durationMs()` becomes valid and listeners
         * fire once. Streaming/network sources are out of scope for now —
         * `path` is opened directly as a local file URL.
         */
        void setSource(std::string path);

        // ------------------------------------------------------------------
        // Playback control
        // ------------------------------------------------------------------

        void play();
        void pause();

        /** @brief Seeks to an absolute position. Clamped to [0, durationMs()]. */
        void seekTo(double position_ms);

        // ------------------------------------------------------------------
        // State
        // ------------------------------------------------------------------

        bool   isPlaying()  const noexcept { return playing_; }
        double positionMs() const noexcept { return position_ms_; }
        double durationMs() const noexcept { return duration_ms_; }

        /** @brief True once `setSource()`'s asset has finished loading. */
        bool isReady() const noexcept { return ready_; }

        /** @brief True while a `VideoPlayer`'s render object is attached. */
        bool hasClients() const noexcept { return attached_ != nullptr; }

        // ------------------------------------------------------------------
        // Listener API
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback that fires whenever playback state,
         * position, or readiness changes.
         * @return A listener ID for use with removeListener().
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Removes the listener with the given ID. */
        void removeListener(uint64_t id);

        // ------------------------------------------------------------------
        // Called by RenderVideoPlayer
        // ------------------------------------------------------------------

        /**
         * @brief Called when a `RenderVideoPlayer` mounts.
         *
         * Unlike `ScrollController` (which only tracks a boolean and relies
         * on the render object to pull state each paint), this controller
         * is what *drives* updates — it pulls decoded frames on a
         * `TickerScheduler` subscription and pushes them into the attached
         * render object's texture, then calls `markNeedsPaint()` on it
         * directly. That push model needs an actual pointer back, not just
         * an attached flag. Safe as a raw pointer: `detach()` is guaranteed
         * to run (from the render object's destructor path) before the
         * object it points to is destroyed.
         */
        void attach(RenderVideoPlayer* render_object) noexcept { attached_ = render_object; }

        /** @brief Called when the `RenderVideoPlayer` unmounts. */
        void detach(RenderVideoPlayer* render_object) noexcept
        {
            if (attached_ == render_object) attached_ = nullptr;
        }

    private:
        void notifyListeners();

        /**
         * @brief Subscribes to `TickerScheduler` if not already subscribed.
         *
         * Called from `setSource()` (to discover readiness/duration — see
         * `onTick()`) and from `play()` (in case `play()` is called before
         * a still-loading source becomes ready). `onTick()` itself decides
         * when to unsubscribe, once there's nothing left to poll for.
         */
        void ensureTicking();

        /**
         * @brief Per-frame work, run on the main thread via
         * `TickerScheduler`. Polls readiness/duration once, then — while
         * playing — pulls the current decoded frame and pushes it to the
         * attached `RenderVideoPlayer`. Unsubscribes itself once there's
         * nothing left to do (ready and paused, or no source).
         */
        void onTick(uint64_t now_ms);

        /**
         * @brief Called (via an `AVPlayerItemDidPlayToEndTimeNotification`
         * observer on macOS) when the current source finishes playing.
         *
         * Pauses, seeks back to the start, and stops ticking — without
         * this, nothing ever notices playback ended: `onTick()` only pulls
         * frames and checks readiness, so `playing_` would stay `true`
         * forever (a stuck Play/Pause button) while the ticker kept
         * uselessly re-requesting frames every tick with nothing new to
         * show (continuous, pointless redraws).
         */
        void onPlaybackEnded();

        uint64_t ticker_id_ = 0;

        bool   playing_     = false;
        bool   ready_       = false;
        double position_ms_ = 0.0;
        double duration_ms_ = 0.0;

        RenderVideoPlayer* attached_ = nullptr;

        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;

        // Opaque storage for native player state (AVPlayer/
        // AVPlayerItemVideoOutput on macOS) — not a backend interface with
        // virtual dispatch (there's only ever one concrete implementation
        // linked in, chosen at build time by which platform directory's
        // .mm/.cpp actually gets compiled, same as HttpClient), just a
        // pImpl so Objective-C types never have to appear in this header.
        // `Impl` is defined only in the platform .mm/.cpp, which is also
        // where setSource()/play()/pause()/seekTo() are actually
        // implemented.
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace systems::leal::campello_widgets
