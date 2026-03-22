#pragma once

#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <cstdint>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Applies an easing curve to an AnimationController's normalised value.
     *
     * CurvedAnimation wraps an existing controller — it does not own a ticker
     * itself. Its `value()` is `curve(controller.normalizedValue())`.
     *
     * Listeners registered on CurvedAnimation are forwarded to the underlying
     * controller so they fire at the same cadence.
     *
     * @code
     * auto ctrl = std::make_shared<AnimationController>(300.0);
     * CurvedAnimation curved(*ctrl, Curves::easeInOut);
     *
     * auto builder = std::make_shared<AnimatedBuilder>();
     * builder->animation = ctrl;
     * builder->builder = [&](BuildContext&) {
     *     double v = curved.value();
     *     ...
     * };
     * @endcode
     */
    class CurvedAnimation
    {
    public:
        using CurveFn = std::function<double(double)>;

        /**
         * @param parent  The controller that drives this animation.
         * @param curve   A function mapping normalised [0,1] → [0,1]
         *                (use a static method from `Curves`, e.g. `Curves::easeInOut`).
         */
        CurvedAnimation(AnimationController& parent, CurveFn curve);

        ~CurvedAnimation();

        // Non-copyable — holds listener IDs on the parent controller.
        CurvedAnimation(const CurvedAnimation&)            = delete;
        CurvedAnimation& operator=(const CurvedAnimation&) = delete;

        // ------------------------------------------------------------------
        // Value
        // ------------------------------------------------------------------

        /**
         * @brief The curved value: `curve(parent.normalizedValue())`.
         *
         * Nominally in [0, 1] but may overshoot for curves like `elasticOut`.
         */
        double value() const;

        AnimationStatus status() const;

        // ------------------------------------------------------------------
        // Listeners
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback that fires whenever the parent value changes.
         *
         * The callback is actually registered on the parent controller; this
         * method returns the parent's listener ID for later removal.
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Removes a listener by the ID returned from `addListener()`. */
        void removeListener(uint64_t id);

    private:
        AnimationController& parent_;
        CurveFn              curve_;
    };

} // namespace systems::leal::campello_widgets
