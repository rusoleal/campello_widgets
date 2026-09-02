#pragma once

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Routes a genuinely user-driven scroll delta between an "outer"
     * and one "inner" scrollable sharing one drag/wheel gesture stream --
     * the coordination mechanism behind NestedScrollView.
     *
     * Deliberately minimal: just the two type-erased apply-callbacks
     * applyUserOffset() needs, bound by whoever constructs one to the
     * outer/inner RenderObject's own applyExternalScrollDelta() member
     * function. No shared base class across RenderListView/
     * RenderSingleChildScrollView/RenderViewport is needed for this --
     * consistent with those three classes already independently duplicating
     * setController()/setPhysics()/scrollOffset() with zero common base.
     *
     * Sign convention matches this codebase's own applyScrollDelta()/
     * applyExternalScrollDelta(): positive delta means the scroll position
     * increases -- the direction that collapses a header. Verified against
     * Flutter's real _NestedScrollCoordinator.applyUserOffset() source
     * (flutter/flutter@master, nested_scroll_view.dart) and translated into
     * this codebase's sign convention, which is the negated raw drag delta
     * Flutter's own method receives -- see this class's .cpp for the
     * translated rule.
     *
     * Two deliberate simplifications from Flutter's real algorithm, both
     * consistent with decisions already made in the NestedScrollView scoping
     * artifact, not new scope cuts:
     *  - Flutter drains any inner position already sitting in negative
     *    overscroll before touching outer, in the collapsing direction --
     *    omitted here since that needs raw unclamped overscroll observation
     *    this codebase can only get via ScrollController's async
     *    addOverscrollListener() channel, not a synchronous read.
     *  - Flutter generalizes the leftover computation across a *list* of
     *    inner positions via a max() -- moot here, since this slice
     *    deliberately targets exactly one inner scrollable.
     */
    class NestedScrollCoordinator
    {
    public:
        /** Bound to the outer scrollable's own applyExternalScrollDelta(). */
        std::function<float(float)> apply_to_outer;
        /** Bound to the inner scrollable's own applyExternalScrollDelta(). */
        std::function<float(float)> apply_to_inner;

        /**
         * @brief Splits a genuinely user-driven delta between outer and
         * inner, per the collapsing/expanding priority rule (see this
         * class's own doc). Assign the same NestedScrollCoordinator to both
         * participants' external_delta_redirect field so either one's own
         * gesture routes through here uniformly -- this method doesn't need
         * to know or care which participant's gesture originated the call.
         */
        void applyUserOffset(float delta);
    };

} // namespace systems::leal::campello_widgets
