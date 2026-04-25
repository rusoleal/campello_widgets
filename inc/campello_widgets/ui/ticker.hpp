#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Per-frame tick pump for the animation system.
     *
     * TickerScheduler follows the same static active-instance pattern as
     * PointerDispatcher. The Renderer calls `tick(now_ms)` once per frame,
     * which forwards the timestamp to every registered callback.
     *
     * AnimationController registers a callback via `subscribe()` when it starts
     * animating and calls `unsubscribe()` when the animation completes or stops.
     */
    class TickerScheduler
    {
    public:
        // ------------------------------------------------------------------
        // Singleton-style active instance (mirrors PointerDispatcher pattern)
        // ------------------------------------------------------------------

        static TickerScheduler* active() noexcept;
        static void setActive(TickerScheduler* scheduler) noexcept;

        // ------------------------------------------------------------------
        // Frame pump — called by Renderer each frame
        // ------------------------------------------------------------------

        /**
         * @brief Dispatches the current timestamp to all registered callbacks.
         *
         * @param now_ms Monotonic timestamp in milliseconds
         *               (from std::chrono::steady_clock).
         */
        void tick(uint64_t now_ms);

        // ------------------------------------------------------------------
        // Subscription API — used by AnimationController
        // ------------------------------------------------------------------

        /**
         * @brief Registers a per-frame callback.
         *
         * @param callback Invoked each frame with the current timestamp in ms.
         * @return A subscription ID that can be passed to `unsubscribe()`.
         */
        uint64_t subscribe(std::function<void(uint64_t)> callback);

        /**
         * @brief Cancels a previously registered callback.
         *
         * Safe to call from within a tick callback (deferred removal).
         *
         * @param id The ID returned by `subscribe()`.
         */
        void unsubscribe(uint64_t id);

    private:
        uint64_t next_id_ = 1;
        std::unordered_map<uint64_t, std::function<void(uint64_t)>> listeners_;

        static TickerScheduler* s_active_;
    };

} // namespace systems::leal::campello_widgets
