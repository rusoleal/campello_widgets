#pragma once

#include <cstdint>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An RGBA color with float components in the range [0, 1].
     */
    struct Color
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        static constexpr Color fromRGBA(float r, float g, float b, float a) noexcept
        {
            return {r, g, b, a};
        }

        static constexpr Color fromRGB(float r, float g, float b) noexcept
        {
            return {r, g, b, 1.0f};
        }

        /** @brief Construct from a packed 0xAARRGGBB 32-bit integer. */
        static constexpr Color fromARGB(uint32_t argb) noexcept
        {
            return {
                ((argb >> 16) & 0xFF) / 255.0f,
                ((argb >>  8) & 0xFF) / 255.0f,
                ((argb >>  0) & 0xFF) / 255.0f,
                ((argb >> 24) & 0xFF) / 255.0f,
            };
        }

        // ------------------------------------------------------------------
        // Common colors
        // ------------------------------------------------------------------

        static constexpr Color transparent()  noexcept { return {0, 0, 0, 0}; }
        static constexpr Color black()        noexcept { return {0, 0, 0, 1}; }
        static constexpr Color white()        noexcept { return {1, 1, 1, 1}; }
        static constexpr Color red()          noexcept { return {1, 0, 0, 1}; }
        static constexpr Color green()        noexcept { return {0, 1, 0, 1}; }
        static constexpr Color blue()         noexcept { return {0, 0, 1, 1}; }

        // ------------------------------------------------------------------

        bool operator==(const Color&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
