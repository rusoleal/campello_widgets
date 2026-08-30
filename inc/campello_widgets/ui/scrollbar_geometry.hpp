#pragma once

#include <algorithm>

namespace systems::leal::campello_widgets
{

    /** @brief A scrollbar thumb's size and position along the scroll axis. */
    struct ScrollbarThumbGeometry
    {
        float length   = 0.0f; ///< Thumb extent along the scroll axis.
        float position = 0.0f; ///< Thumb's offset from the track's start.
    };

    /**
     * @brief Computes a proportional scrollbar thumb's length and position.
     *
     * Standard scrollbar proportion: the thumb occupies
     * `viewport / (viewport + contentRange)` of the track (clamped to at
     * least `min_thumb_length` so it never shrinks to invisibility on very
     * long content), and its position within the remaining track space
     * tracks `offset` linearly between `min_scroll_extent` and
     * `max_scroll_extent`.
     *
     * @param viewport_extent   Visible size along the scroll axis -- also
     *                          the track's own length (the thumb travels
     *                          within the same span the viewport occupies).
     * @param min_scroll_extent `ScrollController::minScrollExtent()`.
     * @param max_scroll_extent `ScrollController::maxScrollExtent()`.
     * @param offset            `ScrollController::offset()`.
     * @param min_thumb_length  Floor on the thumb's rendered length.
     */
    inline ScrollbarThumbGeometry computeScrollbarThumbGeometry(
        float viewport_extent,
        float min_scroll_extent,
        float max_scroll_extent,
        float offset,
        float min_thumb_length = 24.0f)
    {
        if (viewport_extent <= 0.0f)
            return {0.0f, 0.0f};

        const float content_range = std::max(0.0f, max_scroll_extent - min_scroll_extent);
        if (content_range <= 0.0f)
            // Nothing to scroll -- thumb fills the whole track.
            return {viewport_extent, 0.0f};

        const float proportion   = viewport_extent / (viewport_extent + content_range);
        // effective_min: std::clamp(lo, hi) requires lo <= hi -- a caller-supplied
        // min_thumb_length bigger than the viewport itself (a very short/narrow
        // scrollable) would violate that precondition otherwise.
        const float effective_min = std::min(min_thumb_length, viewport_extent);
        const float thumb_length  = std::clamp(proportion * viewport_extent, effective_min, viewport_extent);
        const float track_room   = viewport_extent - thumb_length;
        const float scroll_t     = std::clamp((offset - min_scroll_extent) / content_range, 0.0f, 1.0f);

        return {thumb_length, scroll_t * track_room};
    }

} // namespace systems::leal::campello_widgets
