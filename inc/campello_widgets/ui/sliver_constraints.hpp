#pragma once

#include <campello_widgets/ui/axis.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Whether a sliver's content is ordered forward or reverse
     * relative to the viewport's axis direction.
     *
     * Lets a future RenderViewport grow slivers both up and down from a
     * center anchor — every sliver in this codebase today implicitly
     * assumes `forward` (see the Sliver Protocol Scoping artifact's
     * deferred-items list).
     */
    enum class GrowthDirection
    {
        forward,
        reverse,
    };

    /**
     * @brief Which way the user is *currently* trying to scroll, relative
     * to the viewport's axis/growth direction.
     *
     * Distinct from any actual scroll delta having been applied yet — a
     * floating persistent header (a later stage) uses this to know a
     * reveal gesture is in progress before any content has visibly moved.
     */
    enum class UserScrollDirection
    {
        idle,
        forward,
        reverse,
    };

    /**
     * @brief What a RenderViewport hands down to each RenderSliver child
     * during layout — the sliver-protocol analog of BoxConstraints.
     *
     * Mirrors Flutter's real `SliverConstraints` field-for-field (see the
     * Sliver Protocol Scoping artifact, Part 1) with two fields
     * deliberately omitted: `axisDirection`/`crossAxisDirection`. Those
     * only matter once a RenderViewport exists to interpret them for
     * reverse/horizontal layouts (a later stage) — every field kept here
     * is meaningful and testable against a bare RenderSliver today,
     * without a viewport at all.
     */
    struct SliverConstraints
    {
        /** Which axis (horizontal/vertical) scrolling happens on. */
        Axis axis = Axis::vertical;

        /** See GrowthDirection's doc. */
        GrowthDirection growth_direction = GrowthDirection::forward;

        /** See UserScrollDirection's doc. */
        UserScrollDirection user_scroll_direction = UserScrollDirection::idle;

        /**
         * How far the viewport has already scrolled past *this sliver's
         * own* leading edge, in this sliver's own coordinate system — 0
         * means the sliver's start sits right at the visible edge.
         */
        float scroll_offset = 0.0f;

        /**
         * Total scroll distance already consumed by every sliver before
         * this one — how a sliver learns its own absolute position in the
         * whole scrollable without inspecting its siblings directly.
         */
        float preceding_scroll_extent = 0.0f;

        /**
         * Pixels between the current scroll position and the first pixel
         * not yet painted by an earlier sliver — the actual mechanism a
         * pinned header (a later stage) uses to let content slide
         * underneath it.
         */
        float overlap = 0.0f;

        /** Pixels of paint budget left for this sliver and everything after it. */
        float remaining_paint_extent = 0.0f;

        /** Like remaining_paint_extent, but for the larger off-screen cache region. */
        float remaining_cache_extent = 0.0f;

        /** Where the cache region starts, relative to scroll_offset (usually negative). */
        float cache_origin = 0.0f;

        /**
         * Total viewport size along the main axis — not just what's left.
         * What a fill-viewport-style sliver (a later stage) would size one
         * "page" against.
         */
        float viewport_main_axis_extent = 0.0f;

        /** The viewport's size in the cross axis (width, for a vertical list). */
        float cross_axis_extent = 0.0f;

        bool operator==(const SliverConstraints&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
