#pragma once

#include <cmath>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Collection of easing curve functions.
     *
     * Each function maps a normalised time `t` in [0, 1] to an output value,
     * also nominally in [0, 1] (some curves like `elasticOut` may overshoot).
     *
     * Pass these as the `curve` argument to CurvedAnimation:
     * @code
     * CurvedAnimation anim(controller, Curves::easeInOut);
     * @endcode
     */
    struct Curves
    {
        /** @brief Output equals input — no easing. */
        static double linear(double t) noexcept
        {
            return t;
        }

        /** @brief Slow start, fast end. */
        static double easeIn(double t) noexcept
        {
            return t * t;
        }

        /** @brief Fast start, slow end. */
        static double easeOut(double t) noexcept
        {
            return t * (2.0 - t);
        }

        /** @brief Slow start and end, fast middle. */
        static double easeInOut(double t) noexcept
        {
            return t < 0.5 ? 2.0 * t * t
                           : -1.0 + (4.0 - 2.0 * t) * t;
        }

        /** @brief Cubic ease-in (stronger acceleration). */
        static double easeInCubic(double t) noexcept
        {
            return t * t * t;
        }

        /** @brief Cubic ease-out (stronger deceleration). */
        static double easeOutCubic(double t) noexcept
        {
            const double u = 1.0 - t;
            return 1.0 - u * u * u;
        }

        /** @brief Bounces at the end. */
        static double bounceOut(double t) noexcept
        {
            if (t < 1.0 / 2.75)
                return 7.5625 * t * t;
            if (t < 2.0 / 2.75)
            {
                const double u = t - 1.5 / 2.75;
                return 7.5625 * u * u + 0.75;
            }
            if (t < 2.5 / 2.75)
            {
                const double u = t - 2.25 / 2.75;
                return 7.5625 * u * u + 0.9375;
            }
            const double u = t - 2.625 / 2.75;
            return 7.5625 * u * u + 0.984375;
        }

        /** @brief Elastic overshoot at the end. */
        static double elasticOut(double t) noexcept
        {
            if (t == 0.0 || t == 1.0) return t;
            constexpr double p = 0.3;
            return std::pow(2.0, -10.0 * t) *
                   std::sin((t - p / 4.0) * (2.0 * M_PI) / p) + 1.0;
        }
    };

} // namespace systems::leal::campello_widgets
