#pragma once

#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief What a RenderSliver reports back to its RenderViewport after
     * laying itself out — the sliver-protocol analog of Size.
     *
     * Mirrors Flutter's real `SliverGeometry` field-for-field (see the
     * Sliver Protocol Scoping artifact, Part 1). Every field here is
     * consumed simultaneously by a RenderViewport's layout loop in a later
     * stage — there is no subset that can be built and proven correct in
     * isolation the way most other work in this codebase has been staged.
     */
    struct SliverGeometry
    {
        /**
         * This sliver's total scrollable length, whether currently visible
         * or not — accumulates into the viewport's overall scroll range.
         */
        float scroll_extent = 0.0f;

        /** Pixels this sliver is actually painting right now, given its budget. */
        float paint_extent = 0.0f;

        /**
         * Where the visible part starts, relative to this sliver's own
         * layout position — how a pinned header (a later stage) stays
         * glued to the edge.
         */
        float paint_origin = 0.0f;

        /**
         * Distance from this sliver's start to the *next* sliver's start.
         * Normally equals paint_extent — a pinned header decouples the two
         * so it keeps consuming scroll-layout space while visually clamped
         * at the edge. This one field *is* the pinning mechanism.
         */
        float layout_extent = 0.0f;

        /** What paint_extent would be given an infinite budget — this sliver's full natural size. */
        float max_paint_extent = 0.0f;

        /**
         * How much space this sliver permanently occupies if pinned at an
         * edge — how siblings/the viewport know a persistent header is
         * there and should size around it.
         */
        float max_scroll_obstruction_extent = 0.0f;

        /**
         * How far hit-testing should extend, from where painting started.
         * Concrete slivers set this explicitly, conventionally equal to
         * paint_extent unless intentionally different — mirrors Flutter's
         * `hitTestExtent ?? paintExtent` default-parameter behavior, which
         * C++ has no direct equivalent for on a plain aggregate field.
         */
        float hit_test_extent = 0.0f;

        /** How much of the cache budget this sliver consumed. */
        float cache_extent = 0.0f;

        /**
         * Non-null tells the *parent viewport* "redo this whole layout
         * pass with the scroll offset adjusted by this much" — the escape
         * hatch a sliver uses when it discovers mid-layout that the
         * assumed scroll position was wrong (e.g. a pinned header whose
         * collapse changed things). Not an edge case in Flutter's real
         * viewport — pinned headers depend on it.
         */
        std::optional<float> scroll_offset_correction;

        /** Whether to paint at all. */
        bool visible = true;

        /** Whether this sliver is clipping/overflowing — affects whether the viewport needs to insert a clip. */
        bool has_visual_overflow = false;

        bool operator==(const SliverGeometry&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
