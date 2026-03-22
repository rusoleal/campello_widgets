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
     * momentum decays over time.
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
         * @brief Decays a scroll velocity over one tick.
         *
         * @param velocity   Current velocity in logical pixels per second.
         * @param dt_seconds Elapsed time since the last tick, in seconds.
         * @return New velocity after friction is applied.
         */
        virtual float applyFriction(float velocity, float dt_seconds) const noexcept = 0;

        /** @brief Whether momentum scrolling is allowed at all. */
        virtual bool allowsMomentum() const noexcept = 0;
    };

    // -------------------------------------------------------------------------

    /**
     * @brief Hard-clamped physics — no overscroll.
     *
     * Position is clamped to [min, max]. Momentum decays exponentially and
     * stops immediately at boundaries.
     */
    class ClampingScrollPhysics final : public ScrollPhysics
    {
    public:
        float applyBoundaryConditions(
            float position,
            float min_scroll_extent,
            float max_scroll_extent) const noexcept override
        {
            return std::clamp(position, min_scroll_extent, max_scroll_extent);
        }

        float applyFriction(float velocity, float dt_seconds) const noexcept override
        {
            // Exponential decay — roughly halves every ~0.15 s.
            return velocity * std::pow(0.01f, dt_seconds);
        }

        bool allowsMomentum() const noexcept override { return true; }
    };

    // -------------------------------------------------------------------------

    /**
     * @brief Bouncing physics — allows overscroll that springs back.
     *
     * When the raw position exceeds a boundary, 50 % resistance is applied so
     * overshoot is limited. The tick handler in the render object is responsible
     * for applying a spring-back force once velocity falls low enough.
     */
    class BouncingScrollPhysics final : public ScrollPhysics
    {
    public:
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
                return min_scroll_extent - over * 0.5f;
            }
            if (position > max_scroll_extent)
            {
                const float over = position - max_scroll_extent;
                return max_scroll_extent + over * 0.5f;
            }
            return position;
        }

        float applyFriction(float velocity, float dt_seconds) const noexcept override
        {
            return velocity * std::pow(0.01f, dt_seconds);
        }

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

        float applyFriction(float /*velocity*/, float /*dt_seconds*/) const noexcept override
        {
            return 0.0f;
        }

        bool allowsMomentum() const noexcept override { return false; }
    };

} // namespace systems::leal::campello_widgets
