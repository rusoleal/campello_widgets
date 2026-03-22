#pragma once

#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /** @brief Whether a shape is filled or stroked. */
    enum class PaintStyle
    {
        fill,   ///< Fill the interior of the shape.
        stroke, ///< Draw the outline of the shape.
    };

    /**
     * @brief Describes how a shape should be drawn.
     *
     * Passed to PaintContext drawing operations to control color,
     * fill/stroke style, and stroke width.
     */
    struct Paint
    {
        /** @brief The color to use for fill or stroke. */
        Color color = Color::black();

        /** @brief Whether to fill or stroke the shape. */
        PaintStyle style = PaintStyle::fill;

        /** @brief Stroke width in logical pixels (only used when style == stroke). */
        float stroke_width = 1.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        static Paint filled(Color color) noexcept
        {
            return {color, PaintStyle::fill, 0.0f};
        }

        static Paint stroked(Color color, float width = 1.0f) noexcept
        {
            return {color, PaintStyle::stroke, width};
        }

        bool operator==(const Paint&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
