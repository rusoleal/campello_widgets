#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <utility>

namespace systems::leal::campello_widgets
{

    enum class AnimationStatus
    {
        dismissed, ///< Stopped at the lower bound (or reset).
        forward,   ///< Animating toward the upper bound.
        reverse,   ///< Animating toward the lower bound.
        completed, ///< Stopped at the upper bound.
    };

    /**
     * @brief Drives an animated value between `lower` and `upper` over `duration_ms`.
     *
     * On each frame the TickerScheduler calls the controller's private tick
     * callback, which advances `value_` proportionally to the elapsed time and
     * notifies all listeners.
     *
     * Typical use:
     * @code
     * auto ctrl = std::make_shared<AnimationController>(300.0); // 300 ms
     * ctrl->addListener([&]() { setState([]{}); });
     * ctrl->forward();
     * @endcode
     */
    class AnimationController
    {
    public:
        /**
         * @param duration_ms  Total animation duration in milliseconds.
         * @param lower        Value at the start / dismissed end (default 0.0).
         * @param upper        Value at the completed end (default 1.0).
         */
        explicit AnimationController(double duration_ms,
                                     double lower = 0.0,
                                     double upper = 1.0);

        ~AnimationController();

        // Non-copyable — owns a ticker subscription.
        AnimationController(const AnimationController&)            = delete;
        AnimationController& operator=(const AnimationController&) = delete;

        // ------------------------------------------------------------------
        // State
        // ------------------------------------------------------------------

        double          value()          const noexcept { return value_;  }
        AnimationStatus status()         const noexcept { return status_; }
        bool            isAnimating()    const noexcept;

        /**
         * @brief Value normalised to [0, 1] regardless of lower/upper bounds.
         *
         * Used by CurvedAnimation to apply easing curves.
         */
        double normalizedValue() const noexcept;

        // ------------------------------------------------------------------
        // Control
        // ------------------------------------------------------------------

        /**
         * @brief Animate from the current value (or `from`) toward `upper`.
         *
         * @param from  If >= 0 the controller is reset to this value first.
         *              Pass the default (-1) to continue from the current position.
         */
        void forward(double from = -1.0);

        /**
         * @brief Animate from the current value (or `from`) toward `lower`.
         */
        void reverse(double from = -1.0);

        /** @brief Stop the animation at the current value. */
        void stop();

        /** @brief Reset to `lower` without animating. */
        void reset();

        // ------------------------------------------------------------------
        // Listeners
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback that fires on every value change.
         *
         * @return A listener ID that can be passed to `removeListener()`.
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Cancels the listener with the given ID. */
        void removeListener(uint64_t id);

    private:
        void onTick(uint64_t now_ms);
        void startTicker();
        void stopTicker();
        void notifyListeners();

        double          duration_ms_;
        double          lower_;
        double          upper_;
        double          value_;
        AnimationStatus status_      = AnimationStatus::dismissed;

        // Ticker state
        uint64_t ticker_id_   = 0;     ///< 0 = not subscribed
        uint64_t last_tick_ms_ = 0;    ///< 0 = first tick pending

        // Listeners: (id, callback) pairs
        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;
    };

} // namespace systems::leal::campello_widgets
