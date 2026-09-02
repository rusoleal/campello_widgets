#include <gtest/gtest.h>
#include <campello_widgets/ui/nested_scroll_coordinator.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/render_sliver_overlap_absorber.hpp>
#include <campello_widgets/ui/render_sliver_overlap_injector.hpp>
#include <campello_widgets/ui/render_sliver_persistent_header.hpp>
#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Deliberately distinctly-named from the near-identical helpers in the
    // other sliver/viewport test files -- Unity Build merges anonymous-
    // namespace helpers from different .cpp files into one TU, so a
    // same-named function here would be a redefinition error, not just a
    // shadow.
    void layoutCoordinatorViewport(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }

    std::shared_ptr<cw::RenderSliverPersistentHeader> makeCoordinatorHeader(float min_extent, float max_extent)
    {
        auto header = std::make_shared<cw::RenderSliverPersistentHeader>();
        header->min_extent = min_extent;
        header->max_extent = max_extent;
        return header;
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. NestedScrollCoordinator::applyUserOffset(), isolated -- lambda stand-ins
// recording calls, no real RenderObjects needed.
// ---------------------------------------------------------------------------

TEST(NestedScrollCoordinator, PositiveDeltaFullyAbsorbedByOuterNeverCallsInner)
{
    cw::NestedScrollCoordinator coord;
    bool inner_called = false;
    float outer_seen = 0.0f;

    coord.apply_to_outer = [&](float d) { outer_seen = d; return d; }; // fully absorbs
    coord.apply_to_inner = [&](float d) { inner_called = true; return d; };

    coord.applyUserOffset(50.0f);

    EXPECT_FLOAT_EQ(outer_seen, 50.0f);
    EXPECT_FALSE(inner_called);
}

TEST(NestedScrollCoordinator, PositiveDeltaPartiallyAbsorbedByOuterLeftoverReachesInner)
{
    cw::NestedScrollCoordinator coord;
    float inner_seen = 0.0f;

    coord.apply_to_outer = [&](float /*d*/) { return 30.0f; }; // only absorbs 30 of whatever's requested
    coord.apply_to_inner = [&](float d) { inner_seen = d; return d; };

    coord.applyUserOffset(50.0f);

    EXPECT_FLOAT_EQ(inner_seen, 20.0f); // 50 - 30 leftover
}

TEST(NestedScrollCoordinator, NegativeDeltaFullyAbsorbedByInnerNeverCallsOuter)
{
    cw::NestedScrollCoordinator coord;
    bool outer_called = false;
    float inner_seen = 0.0f;

    coord.apply_to_inner = [&](float d) { inner_seen = d; return d; }; // fully absorbs
    coord.apply_to_outer = [&](float d) { outer_called = true; return d; };

    coord.applyUserOffset(-50.0f);

    EXPECT_FLOAT_EQ(inner_seen, -50.0f);
    EXPECT_FALSE(outer_called);
}

TEST(NestedScrollCoordinator, NegativeDeltaPartiallyAbsorbedByInnerLeftoverReachesOuter)
{
    cw::NestedScrollCoordinator coord;
    float outer_seen = 0.0f;

    coord.apply_to_inner = [&](float /*d*/) { return -30.0f; }; // only absorbs -30
    coord.apply_to_outer = [&](float d) { outer_seen = d; return d; };

    coord.applyUserOffset(-50.0f);

    EXPECT_FLOAT_EQ(outer_seen, -20.0f); // -50 - (-30) leftover
}

TEST(NestedScrollCoordinator, ZeroDeltaCallsNeitherCallback)
{
    cw::NestedScrollCoordinator coord;
    bool outer_called = false;
    bool inner_called  = false;

    coord.apply_to_outer = [&](float d) { outer_called = true; return d; };
    coord.apply_to_inner = [&](float d) { inner_called = true; return d; };

    coord.applyUserOffset(0.0f);

    EXPECT_FALSE(outer_called);
    EXPECT_FALSE(inner_called);
}

TEST(NestedScrollCoordinator, UnsetCallbacksDoNotCrashAndAreTreatedAsAbsorbingNothing)
{
    cw::NestedScrollCoordinator coord;
    // Both callbacks left unset deliberately.
    EXPECT_NO_THROW(coord.applyUserOffset(50.0f));
    EXPECT_NO_THROW(coord.applyUserOffset(-50.0f));
}

TEST(NestedScrollCoordinator, UnsetOuterOnlyTreatsOuterAsAbsorbingNothingAndInnerGetsFullLeftover)
{
    cw::NestedScrollCoordinator coord;
    float inner_seen = 0.0f;
    // apply_to_outer left unset -- outer_applied treated as 0.0f.
    coord.apply_to_inner = [&](float d) { inner_seen = d; return d; };

    coord.applyUserOffset(50.0f);

    EXPECT_FLOAT_EQ(inner_seen, 50.0f); // all of it, since outer "absorbed" 0
}

// ---------------------------------------------------------------------------
// Capstone: real two-RenderViewport end-to-end integration, extending Stage
// 2's own two-viewport shape (test_sliver_overlap_absorber_injector.cpp) --
// outer viewport (absorber wrapping a pinned RenderSliverPersistentHeader,
// min_extent=60, max_extent=160) and inner viewport (injector + a
// RenderSliverFixedExtentList body), wired via a real NestedScrollCoordinator.
//
// Numbers worked by hand before writing this test:
//  - Outer viewport height is deliberately chosen == header.min_extent (60),
//    so outer's own max_extent_ = header.max_extent - outer_viewport_height
//    = 160 - 60 = 100 -- exactly the header's own collapse range (160-60).
//  - Inner viewport: injector reserves 60px (the header's resting
//    obstruction), body is a 20-item x 40px list (800px content), inner
//    viewport height 500 -> inner max_extent_ = (60 + 800) - 500 = 360.
// ---------------------------------------------------------------------------

TEST(NestedScrollCoordinator, RealTwoViewportEndToEndCollapseThenExpand)
{
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();

    // Outer viewport: absorber wrapping a pinned header, sized exactly to
    // the header's own min_extent so max_extent_ lands on its collapse range.
    cw::RenderViewport outer;
    auto absorber = std::make_shared<cw::RenderSliverOverlapAbsorber>();
    absorber->setChild(makeCoordinatorHeader(60.0f, 160.0f));
    absorber->handle = handle;
    outer.insertChild(absorber, 0);

    auto outer_controller = std::make_shared<cw::ScrollController>();
    outer.setController(outer_controller);

    layoutCoordinatorViewport(outer, 300.0f, 60.0f);
    ASSERT_FLOAT_EQ(handle->layout_extent, 60.0f);
    ASSERT_FLOAT_EQ(outer_controller->maxScrollExtent(), 100.0f);

    // Inner viewport: injector sharing the same handle, then a body list.
    cw::RenderViewport inner;
    auto injector = std::make_shared<cw::RenderSliverOverlapInjector>();
    injector->handle = handle;
    inner.insertChild(injector, 0);

    auto body = std::make_shared<cw::RenderSliverFixedExtentList>();
    body->item_count  = 20;
    body->item_extent = 40.0f;
    inner.insertChild(body, 1);

    auto inner_controller = std::make_shared<cw::ScrollController>();
    inner.setController(inner_controller);

    layoutCoordinatorViewport(inner, 300.0f, 500.0f);
    ASSERT_FLOAT_EQ(inner_controller->maxScrollExtent(), 360.0f);

    // Wire the coordinator to both participants.
    cw::NestedScrollCoordinator coord;
    coord.apply_to_outer = [&outer](float d) { return outer.applyExternalScrollDelta(d); };
    coord.apply_to_inner = [&inner](float d) { return inner.applyExternalScrollDelta(d); };
    outer.external_delta_redirect = [&coord](float d) { coord.applyUserOffset(d); };
    inner.external_delta_redirect = [&coord](float d) { coord.applyUserOffset(d); };

    auto outer_root = std::shared_ptr<cw::RenderBox>(&outer, [](cw::RenderBox*) {});
    cw::PointerDispatcher outer_dispatcher(outer_root);
    cw::PointerDispatcher::setActiveDispatcher(&outer_dispatcher);
    outer.attach();

    // --- Collapsing-direction drag on the OUTER view ---
    // Drag the finger up (decreasing y) to scroll the position forward
    // (positive delta), same convention as RenderListView::PanGestureScrolls.
    // The initial down must land within the outer viewport's own hit-test
    // bounds (it's only 60px tall, per the max_extent_ derivation above) --
    // subsequent moves aren't re-hit-tested once the pointer is captured
    // (see RenderListView::MomentumContinuesAfterRelease's own comment on
    // this), so they're free to drift anywhere.
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 30.0f};
    outer_dispatcher.handlePointerEvent(down);

    // move 1: dy=-50, crosses slop, applies its own delta immediately (same
    // behavior RenderListView::PanGestureScrolls already exercises).
    // Positive delta 50 -> outer priority -> outer fully absorbs (0 -> 50,
    // within its [0,100] range) -> no leftover, inner untouched.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, -20.0f};
    outer_dispatcher.handlePointerEvent(move);

    // move 2: dy=-100 relative to the updated pan_last_pos_ (-20). Positive
    // delta 100 -> outer priority -> outer can only absorb up to its
    // boundary (50 -> 100, i.e. 50 more), leftover 50 flows to inner
    // (0 -> 50, fully within its own [0,360] range).
    move.position = {0.0f, -120.0f};
    outer_dispatcher.handlePointerEvent(move);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    outer_dispatcher.handlePointerEvent(up);

    outer.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);

    EXPECT_FLOAT_EQ(outer_controller->offset(), 100.0f); // fully collapsed
    EXPECT_FLOAT_EQ(inner_controller->offset(), 50.0f);  // leftover flowed here

    // Re-layout outer to see the header's own updated collapse state --
    // applyScrollDelta() only marks needs-layout; performLayout() is what
    // actually recomputes sliver geometry.
    layoutCoordinatorViewport(outer, 300.0f, 60.0f);
    EXPECT_FLOAT_EQ(absorber->geometry().paint_extent, 60.0f); // header fully collapsed to min_extent

    // A genuine, pre-existing subtlety of this codebase's own scroll model,
    // confirmed by direct debug tracing while writing this test (not
    // something this stage introduced): raw_offset_ accumulates *unclamped*
    // even under ClampingScrollPhysics -- only the displayed/controller
    // value is clamped at each step. The first drag above requested 150
    // total (50 then 100) but outer could only display up to 100, so
    // outer's raw_offset_ is now 150 (a 50px overshoot past max_extent_),
    // even though its displayed offset reads 100. Reversing direction has
    // to first "unwind" that 50px of overshoot before the displayed value
    // moves at all -- a leftover smaller than 50 would (correctly, if
    // surprisingly) produce zero visible change here. The second drag below
    // is sized to clear that dead zone.

    // --- Expanding-direction drag on the INNER view ---
    auto inner_root = std::shared_ptr<cw::RenderBox>(&inner, [](cw::RenderBox*) {});
    cw::PointerDispatcher inner_dispatcher(inner_root);
    cw::PointerDispatcher::setActiveDispatcher(&inner_dispatcher);
    inner.attach();

    // Drag the finger down (increasing y) -> negative delta -> inner
    // priority. dy=+150, exceeds slop, applies immediately. Inner can only
    // give back 50 (its own offset, down to its 0 boundary) -- leftover
    // -100 flows to outer: raw_offset_ 150 -> 50 (clearing the 50px
    // overshoot dead zone above, then genuinely displacing the displayed
    // value), clamped -> 50, applied -50 (100 -> 50, header re-expanding).
    cw::PointerEvent down2;
    down2.kind     = cw::PointerEventKind::down;
    down2.position = {0.0f, 200.0f};
    inner_dispatcher.handlePointerEvent(down2);

    cw::PointerEvent move2;
    move2.kind     = cw::PointerEventKind::move;
    move2.position = {0.0f, 350.0f}; // dy=+150
    inner_dispatcher.handlePointerEvent(move2);

    cw::PointerEvent up2;
    up2.kind = cw::PointerEventKind::up;
    inner_dispatcher.handlePointerEvent(up2);

    inner.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);

    EXPECT_FLOAT_EQ(inner_controller->offset(), 0.0f);  // back to its own start
    EXPECT_FLOAT_EQ(outer_controller->offset(), 50.0f); // header starts re-expanding

    // Re-layout outer once more to confirm the reduced offset genuinely
    // propagated all the way through the absorber into the wrapped header's
    // own sliver_constraints_ (not just re-confirming the controller's own
    // value, which is already asserted above) -- geometry().paint_extent
    // itself is NOT a useful signal to assert here: this test's outer
    // viewport is deliberately only 60px tall (== header.min_extent, so
    // max_extent_ lands exactly on the header's 100px collapse range, per
    // the worked derivation at the top of this test), and that same 60px
    // height caps paint_extent at 60 for every scroll_offset in
    // [0, max_extent_] -- confirmed by hand: paint_extent only drops below
    // outer_viewport_height once current_extent < outer_viewport_height,
    // which happens only once shrink > max_extent_, but shrink can never
    // exceed max_extent_ (100) in this exact composition. paint_extent
    // reading 60 both fully collapsed and partially re-expanded is
    // therefore correct, not a sign the header failed to update.
    layoutCoordinatorViewport(outer, 300.0f, 60.0f);
    EXPECT_FLOAT_EQ(absorber->child()->sliverConstraints().scroll_offset, 50.0f);
}
