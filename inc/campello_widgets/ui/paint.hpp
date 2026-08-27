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
     * @brief How the two open ends of a stroked line/path are drawn.
     * Mirrors Flutter's `StrokeCap`. Only meaningful for `PaintStyle::stroke`
     * and only at ends that aren't shared with another segment (a closed
     * path/rect has none).
     */
    enum class StrokeCap
    {
        butt,   ///< Ends exactly at the segment's endpoint (default).
        round,  ///< A semicircle of radius `stroke_width / 2` past the endpoint.
        square, ///< A square extension of `stroke_width / 2` past the endpoint.
    };

    /**
     * @brief How two stroked segments meet at a shared vertex. Mirrors
     * Flutter's `StrokeJoin`. Only meaningful for `PaintStyle::stroke` and
     * only where a path/rect outline has an interior vertex (a `drawLine()`
     * has none).
     */
    enum class StrokeJoin
    {
        miter, ///< Segments' outer edges extended to meet at a point (default),
               ///< falling back to `bevel` past `stroke_miter_limit`.
        round, ///< A filled circle of radius `stroke_width / 2` at the vertex.
        bevel, ///< A flat triangle directly connecting the two outer corners.
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
     * @brief Sampling/mipmapping quality for texture-based draws.
     *
     * Mirrors Flutter's `FilterQuality`. `none` selects nearest-neighbor
     * sampling (crisp, blocky when scaled); `low`/`medium`/`high` all
     * currently select bilinear sampling -- this engine has no mipmapping
     * or anisotropic filtering yet, so those three are indistinguishable
     * today. The enum keeps the full Flutter surface for API familiarity
     * and so a future mipmap/anisotropic implementation has where to land.
     */
    enum class FilterQuality
    {
        none,   ///< Nearest-neighbor.
        low,    ///< Bilinear (same as medium/high today).
        medium, ///< Bilinear (same as low/high today).
        high,   ///< Bilinear (same as low/medium today).
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

        /**
         * @brief Whether to invert `color` before drawing (e.g. red -> cyan).
         *
         * Applied once, on the CPU, when the draw command is recorded
         * (`Canvas::drawRect()` etc.) -- correct and free for these solid-
         * color shape fills/strokes, since inverting a single flat color is
         * identical whether done before or after rasterization. Does not
         * apply to `Canvas::drawImage()`/`drawTintedImage()` (per-pixel
         * texture content can't be inverted this way) or to `saveLayer()`'s
         * Paint (the layer's contents aren't a single color) -- those would
         * need real per-pixel GPU support, not implemented yet.
         */
        bool invert_colors = false;

        /**
         * @brief Sampling quality; currently unused by solid-color shape
         * draws (fill/stroke are analytic, not texture-sampled). Kept here
         * for when `Paint` gains a `shader` field (image/gradient fills);
         * until then, use the `filter_quality` parameter on
         * `Canvas::drawImage()`/`drawTintedImage()` directly.
         */
        FilterQuality filter_quality = FilterQuality::high;

        /** @brief Cap style for the open ends of a stroke. See `StrokeCap`. */
        StrokeCap stroke_cap = StrokeCap::butt;

        /** @brief Join style where a stroke's segments meet. See `StrokeJoin`. */
        StrokeJoin stroke_join = StrokeJoin::miter;

        /**
         * @brief Miter join length limit, as a multiple of `stroke_width / 2`.
         * A `miter` join whose point would extend past this falls back to
         * `bevel`. Matches Flutter's `Paint.strokeMiterLimit` default (4.0).
         */
        float stroke_miter_limit = 4.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        static Paint filled(Color color) noexcept
        {
            Paint p;
            p.color = color;
            p.style = PaintStyle::fill;
            return p;
        }

        static Paint stroked(Color color, float width = 1.0f) noexcept
        {
            Paint p;
            p.color        = color;
            p.style        = PaintStyle::stroke;
            p.stroke_width = width;
            return p;
        }

        bool operator==(const Paint&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
