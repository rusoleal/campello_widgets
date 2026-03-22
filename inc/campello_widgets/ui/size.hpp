#pragma once

#include <limits>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A 2D size with a width and height, both in logical pixels.
     */
    struct Size
    {
        float width  = 0.0f;
        float height = 0.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        /** @brief A size of zero in both dimensions. */
        static constexpr Size zero() noexcept { return {0.0f, 0.0f}; }

        /** @brief A size that is infinite in both dimensions. */
        static Size infinite() noexcept
        {
            const float inf = std::numeric_limits<float>::infinity();
            return {inf, inf};
        }

        // ------------------------------------------------------------------
        // Queries
        // ------------------------------------------------------------------

        bool isEmpty()    const noexcept { return width <= 0.0f || height <= 0.0f; }
        bool isInfinite() const noexcept
        {
            const float inf = std::numeric_limits<float>::infinity();
            return width == inf || height == inf;
        }

        // ------------------------------------------------------------------
        // Operators
        // ------------------------------------------------------------------

        bool operator==(const Size&) const noexcept = default;

        Size operator+(const Size& other) const noexcept
        {
            return {width + other.width, height + other.height};
        }

        Size operator-(const Size& other) const noexcept
        {
            return {width - other.width, height - other.height};
        }
    };

} // namespace systems::leal::campello_widgets
