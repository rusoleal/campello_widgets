#pragma once

#include <cmath>
#include <numbers>
#include <functional>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Produces a `Curves`-style damped-sinusoid function approximating
     * spring motion, normalized over a fixed `[0,1]` input/output window.
     *
     * This is a duration-normalized *approximation*, not Flutter's true
     * mass/stiffness/damping-driven variable-duration simulation (which
     * would need a new `AnimationController::animateWith(Simulation)`
     * tick-driving mode -- shared infrastructure every `Animated*` widget
     * uses, not something to change alongside everything else in this
     * batch). Revisit for a true simulation driver if this isn't good
     * enough in practice.
     *
     * @code
     * ctrl->addListener([&] {
     *     const double t = SpringSimulation::curve(0.3)(ctrl->normalizedValue());
     *     ...
     * });
     * @endcode
     */
    struct SpringSimulation
    {
        /**
         * @param damping_ratio 0 = undamped (oscillates for the whole
         *   window), (0,1) = underdamped (overshoots and settles), >=1 =
         *   critically/overdamped (approaches monotonically, no overshoot).
         * @param oscillations  How many full cycles fit in the [0,1] window
         *   for an underdamped curve. Ignored when critically/overdamped.
         */
        static std::function<double(double)> curve(double damping_ratio = 0.5, double oscillations = 3.0)
        {
            damping_ratio           = std::max(0.0, damping_ratio);
            const double omega_n    = std::max(0.0, oscillations) * 2.0 * std::numbers::pi;

            return [damping_ratio, omega_n](double t) -> double
            {
                if (t <= 0.0) return 0.0;
                if (t >= 1.0) return 1.0;

                if (damping_ratio < 1.0)
                {
                    const double omega_d   = omega_n * std::sqrt(1.0 - damping_ratio * damping_ratio);
                    const double envelope  = std::exp(-damping_ratio * omega_n * t);
                    if (omega_d <= 0.0) return 1.0 - envelope; // undamped-frequency edge case
                    return 1.0 - envelope *
                        (std::cos(omega_d * t) + (damping_ratio * omega_n / omega_d) * std::sin(omega_d * t));
                }

                // Critically damped / overdamped: monotonic approach, no overshoot.
                const double envelope = std::exp(-omega_n * t);
                return 1.0 - envelope * (1.0 + omega_n * t);
            };
        }
    };

} // namespace systems::leal::campello_widgets
