#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief Insets (padding or margin) along each edge of a rectangle.
     *
     * All values are in logical pixels and must be non-negative.
     */
    struct EdgeInsets
    {
        float left   = 0.0f;
        float top    = 0.0f;
        float right  = 0.0f;
        float bottom = 0.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        /** @brief Zero insets on all sides. */
        static constexpr EdgeInsets zero() noexcept { return {}; }

        /** @brief The same inset on all four sides. */
        static constexpr EdgeInsets all(float value) noexcept
        {
            return {value, value, value, value};
        }

        /**
         * @brief Symmetric insets.
         * @param vertical   Applied to top and bottom.
         * @param horizontal Applied to left and right.
         */
        static constexpr EdgeInsets symmetric(float vertical, float horizontal) noexcept
        {
            return {horizontal, vertical, horizontal, vertical};
        }

        /**
         * @brief Per-side insets with defaults of zero.
         */
        static constexpr EdgeInsets only(
            float left   = 0.0f,
            float top    = 0.0f,
            float right  = 0.0f,
            float bottom = 0.0f) noexcept
        {
            return {left, top, right, bottom};
        }

        // ------------------------------------------------------------------
        // Helpers
        // ------------------------------------------------------------------

        /** @brief Sum of left and right insets. */
        float horizontal() const noexcept { return left + right; }

        /** @brief Sum of top and bottom insets. */
        float vertical() const noexcept { return top + bottom; }

        bool operator==(const EdgeInsets&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
