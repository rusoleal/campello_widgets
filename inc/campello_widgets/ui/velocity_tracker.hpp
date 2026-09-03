#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Estimates a gesture's release velocity from a short history of
     * (time, position) samples, mirroring Flutter's `VelocityTracker`.
     *
     * A naive "delta between the last two samples, divided by the time
     * between them" estimate is fragile in two common ways: platform event
     * delivery can batch several pointer-move callbacks together (Wayland,
     * X11, and others all do this under load), leaving near-zero wall-clock
     * time between the last few samples even though the underlying gesture
     * spans tens of milliseconds; and ordinary human deceleration right
     * before lifting a finger can make the very last sample-to-sample delta
     * small even when the gesture carried real momentum a few samples
     * earlier. Both leave a fling feeling like it "immediately stops."
     *
     * This instead fits a line (least squares) through the samples still
     * within a short trailing time window of the newest one, which is far
     * more robust to both problems.
     *
     * Samples are timestamped in milliseconds (matching PointerEvent::
     * timestamp_ms and this codebase's other time plumbing, e.g.
     * PointerDispatcher::tick()/currentMonotonicMs()) rather than a
     * std::chrono::steady_clock::time_point -- callers pass the triggering
     * PointerEvent's own timestamp_ms directly, instead of each call site
     * calling steady_clock::now() itself. Calling now() deep inside a
     * recognizer's own event-handling code meant the "timestamp" reflected
     * whatever unrelated processing overhead ran before that line executed,
     * not when the input actually occurred -- harmless in an optimized
     * build, but enough to dilute a fast flick's computed velocity toward
     * zero in an unoptimized Debug build under a loaded CI runner (the
     * horizon window below aging out the real samples before release).
     */
    class VelocityTracker
    {
    public:
        void reset() noexcept
        {
            count_ = 0;
            head_  = 0;
        }

        void addPosition(uint64_t timestamp_ms, float position) noexcept
        {
            samples_[head_] = {timestamp_ms, position};
            head_ = (head_ + 1) % kMaxSamples;
            if (count_ < kMaxSamples) ++count_;
        }

        /**
         * @brief Least-squares velocity, in position units per second, over
         * the retained window. Returns 0 with fewer than two usable samples.
         */
        float getVelocity() const noexcept
        {
            if (count_ < 2) return 0.0f;

            const std::size_t newest_index = (head_ + kMaxSamples - 1) % kMaxSamples;
            const uint64_t     newest_ms    = samples_[newest_index].timestamp_ms;

            // Accumulate a linear regression of position against "seconds
            // before the newest sample" (so the newest sample sits at x=0),
            // walking backward from the newest sample and stopping once a
            // sample falls outside the trailing window — an old, slow part
            // of a long drag shouldn't dilute the velocity of a fast flick
            // at the very end.
            double sum_t = 0.0, sum_p = 0.0, sum_tt = 0.0, sum_tp = 0.0;
            int n = 0;
            for (std::size_t i = 0; i < count_; ++i)
            {
                const std::size_t idx = (head_ + kMaxSamples - 1 - i) % kMaxSamples;
                const Sample& s = samples_[idx];
                const double t  = static_cast<double>(newest_ms - s.timestamp_ms) / 1000.0;
                if (t > kHorizonSeconds) break;

                sum_t  += t;
                sum_p  += s.position;
                sum_tt += t * t;
                sum_tp += t * s.position;
                ++n;
            }
            if (n < 2) return 0.0f;

            const double mean_t = sum_t / n;
            const double mean_p = sum_p / n;
            const double denom  = sum_tt - n * mean_t * mean_t;
            if (std::abs(denom) < 1e-9) return 0.0f;

            // Slope of position over t, where t counts backward from "now"
            // (t=0) — negate to get the forward-time velocity.
            const double slope = (sum_tp - n * mean_t * mean_p) / denom;
            return static_cast<float>(-slope);
        }

    private:
        struct Sample
        {
            uint64_t timestamp_ms = 0;
            float    position     = 0.0f;
        };

        static constexpr std::size_t kMaxSamples     = 20;
        static constexpr double      kHorizonSeconds = 0.1; // 100ms, matches Flutter's default.

        std::array<Sample, kMaxSamples> samples_{};
        std::size_t                     head_  = 0;
        std::size_t                     count_ = 0;
    };

} // namespace systems::leal::campello_widgets
