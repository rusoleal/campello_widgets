#pragma once

namespace systems::leal::campello_widgets
{

    /** @brief How children are distributed along the main axis of a flex layout. */
    enum class MainAxisAlignment
    {
        start,        ///< Pack children at the start.
        end,          ///< Pack children at the end.
        center,       ///< Center children.
        spaceBetween, ///< Evenly space children; no space at edges.
        spaceAround,  ///< Evenly space children; half-space at edges.
        spaceEvenly,  ///< Equal space between every pair of children and the edges.
    };

    /** @brief How children are aligned perpendicular to the main axis. */
    enum class CrossAxisAlignment
    {
        start,   ///< Align to the start of the cross axis.
        end,     ///< Align to the end of the cross axis.
        center,  ///< Center along the cross axis.
        stretch, ///< Stretch children to fill the cross axis.
    };

    /** @brief Whether a flex layout should occupy as much space as possible. */
    enum class MainAxisSize
    {
        min, ///< Be as small as possible in the main axis.
        max, ///< Be as large as possible in the main axis.
    };

} // namespace systems::leal::campello_widgets
