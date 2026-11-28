#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Estimates a gesture's release velocity from a short history of
     * (time, position) samples, mirroring Flutter's `VelocityTracker`
     * (`packages/flutter/lib/src/gestures/velocity_tracker.dart`).
     *
     * Units: positions are logical pixels (every platform host divides raw
     * device coordinates by the device pixel ratio before building a
     * PointerEvent), so the result is logical px/s — the same density-
     * independent unit as Flutter's `pixelsPerSecond`, Android's dp/s and
     * iOS's pt/s.
     *
     * Algorithm (same as Flutter):
     *  - Walk backward from the newest sample, keeping samples while they
     *    are within kHorizonMs (100ms) of the newest one AND no gap between
     *    consecutive samples exceeds kAssumePointerMoveStoppedMs (40ms) —
     *    a longer gap means the pointer paused, so motion before it is not
     *    part of the fling.
     *  - Fit a degree-2 polynomial p(t) = a + b·t + c·t² (least squares,
     *    t = age relative to the newest sample, so t = 0 is "now") and
     *    return b, the slope *at the newest sample*.
     *
     * A degree-1 (straight line) fit returns the *average* velocity over
     * the window — roughly the speed ~50ms before release. Real flicks
     * accelerate right up to lift-off, so a linear fit systematically
     * under-reports the release speed (by ~50% for a typical flick),
     * making momentum start visibly slower than the finger was moving.
     * The quadratic fit tracks that acceleration and reports the speed at
     * the instant of release, so momentum continues the finger's motion
     * without a velocity discontinuity.
     *
     * Robustness to bursty delivery (several moves stamped with the same
     * or nearly the same time) comes from fitting over every sample in
     * the window instead of the last delta.
     *
     * Samples are timestamped with the triggering PointerEvent's own
     * timestamp_ms rather than steady_clock::now() at the call site, so
     * processing overhead before the call does not distort the timing.
     */
    class VelocityTracker
    {
    public:
        /// Gap (ms) after which the pointer is considered to have stopped.
        static constexpr uint64_t kAssumePointerMoveStoppedMs = 40;

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

        /// Timestamp (ms) of the newest sample, or 0 when there are none.
        uint64_t newestTimestampMs() const noexcept
        {
            return count_ == 0 ? 0 : samples_[(head_ + kMaxSamples - 1) % kMaxSamples].timestamp_ms;
        }

        /**
         * @brief Velocity, in logical px/s, at the newest sample.
         *
         * @param now_ms Optional timestamp of the event that ends the
         *     gesture (e.g. the pointer-up's timestamp_ms). If it is more
         *     than kAssumePointerMoveStoppedMs after the newest sample the
         *     pointer was held still before release, and the result is 0
         *     (Flutter's `_sinceLastSample` check). Ignored when 0 or not
         *     later than the newest sample.
         *
         * Returns 0 with fewer than two usable samples. With exactly two
         * (or when the quadratic system is degenerate), falls back to the
         * straight-line slope.
         */
        float getVelocity(uint64_t now_ms = 0) const noexcept
        {
            if (count_ < 2) return 0.0f;

            const std::size_t newest_index = (head_ + kMaxSamples - 1) % kMaxSamples;
            const uint64_t    newest_ms    = samples_[newest_index].timestamp_ms;

            if (now_ms > newest_ms && now_ms - newest_ms > kAssumePointerMoveStoppedMs)
                return 0.0f;

            // Power sums for the normal equations, with t in milliseconds
            // (t <= 0, newest sample at t = 0). Positions are taken
            // relative to the newest sample to keep the sums well scaled.
            const double p0 = samples_[newest_index].position;
            double s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0;
            double y0 = 0, y1 = 0, y2 = 0;
            int n = 0;
            uint64_t previous_ms = newest_ms;
            for (std::size_t i = 0; i < count_; ++i)
            {
                const std::size_t idx = (head_ + kMaxSamples - 1 - i) % kMaxSamples;
                const Sample& s = samples_[idx];
                if (s.timestamp_ms > newest_ms) break; // clock went backwards
                const uint64_t age = newest_ms - s.timestamp_ms;
                const uint64_t gap = previous_ms - s.timestamp_ms;
                if (age > kHorizonMs || gap > kAssumePointerMoveStoppedMs) break;
                previous_ms = s.timestamp_ms;

                const double t  = -static_cast<double>(age);
                const double y  = static_cast<double>(s.position) - p0;
                const double t2 = t * t;
                s0 += 1.0;  s1 += t;  s2 += t2;  s3 += t2 * t;  s4 += t2 * t2;
                y0 += y;    y1 += t * y;  y2 += t2 * y;
                ++n;
            }
            if (n < 2) return 0.0f;

            if (n >= 3)
            {
                // Solve [s0 s1 s2; s1 s2 s3; s2 s3 s4]·[a b c] = [y0 y1 y2]
                // for b by Cramer's rule.
                const double det = s0 * (s2 * s4 - s3 * s3)
                                 - s1 * (s1 * s4 - s3 * s2)
                                 + s2 * (s1 * s3 - s2 * s2);
                // Scale-aware degeneracy check (e.g. only two distinct
                // timestamps among the samples).
                const double scale = s0 * s2 * s4;
                if (scale > 0.0 && std::abs(det) > 1e-9 * scale)
                {
                    const double det_b = s0 * (y1 * s4 - s3 * y2)
                                       - y0 * (s1 * s4 - s3 * s2)
                                       + s2 * (s1 * y2 - y1 * s2);
                    return static_cast<float>(det_b / det * 1000.0);
                }
            }

            // Linear fallback.
            const double denom = s0 * s2 - s1 * s1;
            if (std::abs(denom) < 1e-9) return 0.0f;
            const double slope = (s0 * y1 - s1 * y0) / denom;
            return static_cast<float>(slope * 1000.0);
        }

    private:
        struct Sample
        {
            uint64_t timestamp_ms = 0;
            float    position     = 0.0f;
        };

        static constexpr std::size_t kMaxSamples = 20;  // Flutter's _historySize.
        static constexpr uint64_t    kHorizonMs  = 100; // Flutter's _horizonMilliseconds.

        std::array<Sample, kMaxSamples> samples_{};
        std::size_t                     head_  = 0;
        std::size_t                     count_ = 0;
    };

} // namespace systems::leal::campello_widgets
