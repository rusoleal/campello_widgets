#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief A 2D translation (x, y) in logical pixels.
     *
     * Used to position a child render object relative to its parent's origin.
     */
    struct Offset
    {
        float x = 0.0f;
        float y = 0.0f;

        static constexpr Offset zero() noexcept { return {0.0f, 0.0f}; }

        bool operator==(const Offset&) const noexcept = default;

        Offset operator+(const Offset& other) const noexcept
        {
            return {x + other.x, y + other.y};
        }

        Offset operator-(const Offset& other) const noexcept
        {
            return {x - other.x, y - other.y};
        }
    };

} // namespace systems::leal::campello_widgets
