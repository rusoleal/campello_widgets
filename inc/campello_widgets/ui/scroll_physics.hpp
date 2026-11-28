#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Abstract base for scroll physics simulation.
     *
     * A ScrollPhysics object is a stateless strategy that governs how a
     * scrollable render object responds to boundary conditions and how its
     * momentum decays over time. Distances are logical pixels and
     * velocities logical px/s (density-independent, like Flutter).
     *
     * Standard implementations:
     *  - ClampingScrollPhysics         — hard clamp, no overscroll (default)
     *  - BouncingScrollPhysics         — brief overscroll that springs back
     *  - NeverScrollableScrollPhysics  — disables all scrolling
     */
    class ScrollPhysics
    {
    public:
        virtual ~ScrollPhysics() = default;

        /**
         * @brief Applies boundary conditions to a raw scroll position.
         *
         * @param position          Raw (possibly out-of-bounds) scroll offset.
         * @param min_scroll_extent Minimum valid offset (usually 0).
         * @param max_scroll_extent Maximum valid offset (content − viewport).
         * @return The corrected position.
         */
        virtual float applyBoundaryConditions(
            float position,
            float min_scroll_extent,
            float max_scroll_extent) const noexcept = 0;

        /**
         * @brief Advances the momentum (ballistic) phase by one tick.
         *
         * Integrates this physics' fling simulation exactly over
         * `dt_seconds`, starting from `velocity`: returns the distance
         * travelled during the interval and replaces `velocity` with the
         * velocity at its end. Every simulation used here is memoryless
         * (its future depends only on the current velocity, not on when
         * the fling started), so stepping tick by tick traces exactly the
         * same curve as evaluating Flutter's `Simulation.x(t)` from the
         * release instant, independent of frame rate or dropped frames.
         *
         * Units: logical px and logical px/s (density-independent).
         */
        virtual float advanceMomentum(float& velocity, float dt_seconds) const noexcept = 0;

        /// Release speeds below this (logical px/s) do not fling (Flutter's minFlingVelocity).
        virtual float minFlingVelocity() const noexcept { return kDefaultMinFlingVelocity; }

        /// Release speeds are clamped to this (logical px/s) (Flutter's maxFlingVelocity).
        virtual float maxFlingVelocity() const noexcept { return kDefaultMaxFlingVelocity; }

        /**
         * @brief Turns a raw drag-release velocity into the fling's start
         * velocity: 0 below minFlingVelocity(), clamped to ±maxFlingVelocity().
         */
        float flingVelocity(float release_velocity) const noexcept
        {
            if (!allowsMomentum()) return 0.0f;
            if (std::abs(release_velocity) < minFlingVelocity()) return 0.0f;
            const float max_v = maxFlingVelocity();
            return std::clamp(release_velocity, -max_v, max_v);
        }

        /// Flutter's kMinFlingVelocity (gestures/constants.dart).
        static constexpr float kDefaultMinFlingVelocity = 50.0f;
        /// Flutter's kMaxFlingVelocity (gestures/constants.dart).
        static constexpr float kDefaultMaxFlingVelocity = 8000.0f;

        /** @brief Whether momentum scrolling is allowed at all. */
        virtual bool allowsMomentum() const noexcept = 0;
    };

    // -------------------------------------------------------------------------

    /**
     * @brief Hard-clamped physics — no overscroll (Android feel).
     *
     * Position is clamped to [min, max]. Momentum follows Flutter's
     * `ClampingScrollSimulation`, itself a port of Android's
     * `OverScroller` spline fling: for a fling starting at velocity v the
     * total duration is T(v) = D_RATE·INFLEXION·(|v|/v_ref)^(1/(D_RATE−1))
     * and the position follows x(t) = D·(1 − (1 − t/T)^D_RATE), so the
     * velocity is v(t) = v·(1 − t/T)^(D_RATE−1) — starting exactly at the
     * release velocity and reaching zero at t = T (a fling has a definite
     * end instead of fading out exponentially).
     */
    class ClampingScrollPhysics final : public ScrollPhysics
    {
    public:
        /// Android's mFlingFriction / ViewConfiguration.getScrollFriction().
        static constexpr double kFriction = 0.015;

        float applyBoundaryConditions(
            float position,
            float min_scroll_extent,
            float max_scroll_extent) const noexcept override
        {
            return std::clamp(position, min_scroll_extent, max_scroll_extent);
        }

        float advanceMomentum(float& velocity, float dt_seconds) const noexcept override
        {
            const double v = velocity;
            if (v == 0.0 || dt_seconds <= 0.0f) return 0.0f;

            const double duration = flingDuration(v);
            const double distance = v * duration / kDecelerationRate;
            if (dt_seconds >= duration)
            {
                velocity = 0.0f;
                return static_cast<float>(distance);
            }
            const double remaining = 1.0 - dt_seconds / duration;
            velocity = static_cast<float>(v * std::pow(remaining, kDecelerationRate - 1.0));
            return static_cast<float>(distance * (1.0 - std::pow(remaining, kDecelerationRate)));
        }

        bool allowsMomentum() const noexcept override { return true; }

        /// Total duration (s) of a fling starting at `velocity` (logical px/s).
        static double flingDuration(double velocity) noexcept
        {
            const double reference_velocity = kFriction * kPhysicalCoeff / kInflexion;
            const double android_duration =
                std::pow(std::abs(velocity) / reference_velocity, 1.0 / (kDecelerationRate - 1.0));
            // Flutter finishes a bit sooner than Android to travel the same distance.
            return kDecelerationRate * kInflexion * android_duration;
        }

        /// Total distance (logical px, signed) of a fling starting at `velocity`.
        static double flingDistance(double velocity) noexcept
        {
            return velocity * flingDuration(velocity) / kDecelerationRate;
        }

    private:
        // Android's DECELERATION_RATE = log(0.78) / log(0.9) ≈ 2.358.
        static constexpr double kDecelerationRate = 2.3582018154259448;
        // Android's INFLEXION.
        static constexpr double kInflexion = 0.35;
        // Android's mPhysicalCoeff in logical px/s²: g · in/m · px/in(160) · 0.84.
        static constexpr double kPhysicalCoeff = 9.80665 * 39.37 * 160.0 * 0.84;
    };

    // -------------------------------------------------------------------------

    /**
     * @brief Bouncing physics — allows overscroll that springs back.
     *
     * When the raw position exceeds a boundary, iOS-style rubber-banding
     * resistance is applied: `f(x) = x·d·c / (d + c·x)`, where `x` is the
     * raw distance past the boundary, `c` is a damping constant, and `d`
     * is the displayed overscroll's asymptotic cap. Unlike a flat
     * percentage (e.g. "50% resistance"), which lets the displayed
     * overscroll keep growing *linearly*, without bound, the harder/
     * further you drag, this curve approaches `d` and no further — a
     * fast or long drag stretches the content by an amount bounded by
     * `kMaxOverscroll` (used as `d`), not by how far you dragged, so the
     * spring-back that follows never has to travel an excessive
     * distance. The tick handler in the render object is responsible for
     * applying that spring-back force once velocity falls low enough.
     */
    class BouncingScrollPhysics final : public ScrollPhysics
    {
    public:
        /// Asymptotic cap (px) on displayed overscroll, regardless of raw drag distance.
        static constexpr float kMaxOverscroll = 100.0f;
        /// iOS's rubber-band damping constant.
        static constexpr float kRubberBandCoefficient = 0.55f;

        float applyBoundaryConditions(
            float position,
            float min_scroll_extent,
            float max_scroll_extent) const noexcept override
        {
            if (max_scroll_extent <= min_scroll_extent)
                return min_scroll_extent;

            if (position < min_scroll_extent)
            {
                const float over = min_scroll_extent - position;
                return min_scroll_extent - rubberBand(over);
            }
            if (position > max_scroll_extent)
            {
                const float over = position - max_scroll_extent;
                return max_scroll_extent + rubberBand(over);
            }
            return position;
        }

    private:
        static float rubberBand(float over) noexcept
        {
            return (over * kMaxOverscroll * kRubberBandCoefficient) /
                   (kMaxOverscroll + kRubberBandCoefficient * over);
        }

    public:
        /// UIScrollView.decelerationRate .normal (0.998 per ms): 0.998^1000 ≈ 0.135 per second.
        static constexpr double kDrag = 0.135;

        /**
         * Flutter's `FrictionSimulation(0.135, …)` as used by
         * `BouncingScrollSimulation`: v(t) = v·0.135^t and
         * x(t) = v·(0.135^t − 1)/ln(0.135) — exponential decay at the
         * same rate as a native iOS scroll view.
         */
        float advanceMomentum(float& velocity, float dt_seconds) const noexcept override
        {
            const double v = velocity;
            if (v == 0.0 || dt_seconds <= 0.0f) return 0.0f;
            const double decay = std::pow(kDrag, static_cast<double>(dt_seconds));
            velocity = static_cast<float>(v * decay);
            return static_cast<float>(v * (decay - 1.0) / std::log(kDrag));
        }

        /// Flutter: kMinFlingVelocity · 2 — this simulation glides further,
        /// so it needs a more deliberate flick to start.
        float minFlingVelocity() const noexcept override { return kDefaultMinFlingVelocity * 2.0f; }

        bool allowsMomentum() const noexcept override { return true; }
    };

    // -------------------------------------------------------------------------

    /**
     * @brief Never-scrollable physics — all scroll input is ignored.
     */
    class NeverScrollableScrollPhysics final : public ScrollPhysics
    {
    public:
        float applyBoundaryConditions(
            float /*position*/,
            float min_scroll_extent,
            float /*max_scroll_extent*/) const noexcept override
        {
            return min_scroll_extent;
        }

        float advanceMomentum(float& velocity, float /*dt_seconds*/) const noexcept override
        {
            velocity = 0.0f;
            return 0.0f;
        }

        bool allowsMomentum() const noexcept override { return false; }
    };

} // namespace systems::leal::campello_widgets
