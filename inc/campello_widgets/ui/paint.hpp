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
     * @brief Blend modes for compositing colors.
     * 
     * These are a subset of Flutter's BlendMode enum.
     */
    enum class BlendMode
    {
        clear,       /// Drop both source and destination
        src,         /// Keep source, drop destination
        dst,         /// Keep destination, drop source
        srcOver,     /// Source over destination (default)
        dstOver,     /// Destination over source
        srcIn,       /// Source where destination is
        dstIn,       /// Destination where source is
        srcOut,      /// Source where destination is not
        dstOut,      /// Destination where source is not
        srcATop,     /// Source on top of destination
        dstATop,     /// Destination on top of source
        xorMode,     /// XOR of source and destination
        plus,        /// Sum of source and destination
        modulate,    /// Product of source and destination
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

        /** @brief Blend mode for compositing (default: srcOver). */
        BlendMode blend_mode = BlendMode::srcOver;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        static Paint filled(Color color) noexcept
        {
            return {color, PaintStyle::fill, 0.0f, BlendMode::srcOver};
        }

        static Paint stroked(Color color, float width = 1.0f) noexcept
        {
            return {color, PaintStyle::stroke, width, BlendMode::srcOver};
        }

        bool operator==(const Paint&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
