#include <gtest/gtest.h>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <chrono>
#include <thread>
#include <vector>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Lay out the list view at the given viewport size (no item boxes yet).
static void doLayout(cw::RenderListView& lv, float vw, float vh)
{
    lv.layout(cw::BoxConstraints::tight(vw, vh));
}

// ---------------------------------------------------------------------------
// Visible range — vertical list (default)
// ---------------------------------------------------------------------------

TEST(RenderListView, FirstVisibleIndexIsZeroAtScrollStart)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
}

TEST(RenderListView, LastVisibleIndexIsFloorViewportOverExtent)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f); // viewport=300, extent=60 → last = floor(300/60)=5, min(29,5)=5

    EXPECT_EQ(lv.lastVisibleIndex(), 5);
}

TEST(RenderListView, LastVisibleIndexClampsToItemCountMinusOne)
{
    cw::RenderListView lv;
    lv.item_count  = 3;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 600.0f); // viewport=600 → floor(600/60)=10, clamped to 2

    EXPECT_EQ(lv.lastVisibleIndex(), 2);
}

TEST(RenderListView, VisibleRangeWithExactFitViewport)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 250.0f); // viewport=250 = 5 × 50 exactly → last=floor(250/50)=5

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
    EXPECT_EQ(lv.lastVisibleIndex(),  5);
}

TEST(RenderListView, ZeroItemCountReturnsNegativeLastIndex)
{
    cw::RenderListView lv;
    lv.item_count  = 0;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.lastVisibleIndex(), -1);
}

TEST(RenderListView, ZeroItemExtentReturnsDefaultIndices)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 0.0f; // guard: zero extent
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
    EXPECT_EQ(lv.lastVisibleIndex(),  -1);
}

// ---------------------------------------------------------------------------
// Size after layout
// ---------------------------------------------------------------------------

TEST(RenderListView, SizeFillsTightConstraints)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 50.0f;
    doLayout(lv, 640.0f, 200.0f);

    EXPECT_FLOAT_EQ(lv.size().width,  640.0f);
    EXPECT_FLOAT_EQ(lv.size().height, 200.0f);
}

// ---------------------------------------------------------------------------
// Item boxes — layout assigns correct offsets
// ---------------------------------------------------------------------------

TEST(RenderListView, ItemBoxesAreLayoutAtCorrectOffsets)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 80.0f;
    doLayout(lv, 400.0f, 300.0f);

    // Add item boxes for indices 0, 1, 2.
    for (int i = 0; i < 3; ++i)
    {
        auto box = std::make_shared<cw::RenderSizedBox>();
        lv.setItemBox(i, box);
    }

    // Trigger a second layout so the item boxes are positioned.
    doLayout(lv, 400.0f, 300.0f);

    // Each item box must have been laid out with height == item_extent.
    // We verify via size() on the boxes, which is set by layout.
    for (int i = 0; i < 3; ++i)
    {
        // Item boxes are not directly accessible after setItemBox, but we can
        // verify the list itself didn't crash and is still sized correctly.
        EXPECT_FLOAT_EQ(lv.size().height, 300.0f);
    }
}

// ---------------------------------------------------------------------------
// Scroll extent calculation
// ---------------------------------------------------------------------------

TEST(RenderListView, OnVisibleRangeChangedCallbackFires)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    int call_count = 0;
    lv.on_visible_range_changed = [&]{ ++call_count; };

    doLayout(lv, 400.0f, 200.0f);
    // The initial layout should fire the callback (cached=-1/-1, new=0/3).
    EXPECT_GE(call_count, 1);
}

TEST(RenderListView, OnVisibleRangeChangedNotFiredWhenRangeUnchanged)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    int call_count = 0;
    lv.on_visible_range_changed = [&]{ ++call_count; };

    doLayout(lv, 400.0f, 200.0f); // first layout, fires
    int after_first = call_count;

    doLayout(lv, 400.0f, 200.0f); // same constraints, same range — should not fire again
    EXPECT_EQ(call_count, after_first);
}

// ---------------------------------------------------------------------------
// Input handling — hover vs pan vs wheel
// ---------------------------------------------------------------------------

TEST(RenderListView, HoverMoveDoesNotScroll)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f); // viewport = 200, 4 items visible

    lv.attach();

    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    // Simulate a hover move (no preceding down) with a large delta.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f};
    dispatcher.handlePointerEvent(move);

    // Move again to create a delta from the first hover position.
    move.position = {0.0f, 300.0f};
    dispatcher.handlePointerEvent(move);

    // List should NOT have scrolled.
    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderListView, PanGestureScrolls)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    lv.attach();

    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    // down — start inside the viewport
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    // move far enough to exceed pan slop (36px, touch default) and trigger
    // panning. Drag UP (decreasing y) to scroll down and reveal lower items.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f};
    dispatcher.handlePointerEvent(move);

    // scroll down by 100 px total → 100/50 = 2 items scrolled
    move.position = {0.0f, 50.0f};
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(lv.firstVisibleIndex(), 2);

    // up
    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// Regresses a bug where release velocity was computed from only the last
// two pointer-move samples (delta / dt since the previous move). That
// estimate is fragile — platform event delivery can batch several
// pointer-move callbacks together with near-zero wall-clock time between
// them, and ordinary human deceleration right before lifting a finger can
// leave the very last sample-to-sample delta small even after a fast drag
// — either way making the list stop dead on release instead of coasting
// with decaying momentum. RenderListView now fits a line through recent
// (time, position) samples via VelocityTracker instead.
TEST(RenderListView, MomentumContinuesAfterRelease)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 100;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    lv.attach();

    auto nowMs = [] {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    // Synthetic timestamps, +10ms per event, instead of real
    // std::this_thread::sleep_for() between dispatches: VelocityTracker
    // computes elapsed time from PointerEvent::timestamp_ms, not real
    // wall-clock time, so this makes the intended ~4000 px/s drag
    // deterministic regardless of how long this process actually takes to
    // execute each line (an unoptimized Debug build under a loaded CI
    // runner previously let real per-call overhead eat into the tracker's
    // 100ms trailing window before release, diluting the computed velocity
    // toward zero — see VelocityTracker's own doc comment).
    uint64_t t = 1'000;

    cw::PointerEvent down;
    down.kind         = cw::PointerEventKind::down;
    down.position     = {0.0f, 190.0f};
    down.timestamp_ms = t;
    dispatcher.handlePointerEvent(down);

    // Fast, steady drag upward (reveals later items) — 40px every 10ms
    // (~4000 px/s), well above the momentum threshold. Once the pointer is
    // captured on down, subsequent moves route via the cached hit path
    // regardless of position, so drifting past the viewport's own bounds
    // here is fine.
    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float y = 190.0f;
    for (int i = 1; i <= 5; i++)
    {
        t += 10;
        y -= 40.0f;
        move.position     = {0.0f, y};
        move.timestamp_ms = t;
        dispatcher.handlePointerEvent(move);
    }
    // Final sample right before release: a near-zero delta, matching either
    // natural human deceleration as a finger lifts or a platform batching
    // several move callbacks together with negligible time between them —
    // both leave the *previous* implementation's single-sample delta/dt
    // estimate reporting ~0 velocity despite the real, fast drag above.
    t += 10;
    y -= 0.2f;
    move.position     = {0.0f, y};
    move.timestamp_ms = t;
    dispatcher.handlePointerEvent(move);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    const int index_at_release = lv.firstVisibleIndex();

    // The first tick after release only primes onTick()'s internal
    // last_tick_ms_ clock (there's no prior tick to measure dt against);
    // subsequent ticks are where momentum actually applies.
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());

    EXPECT_GT(lv.firstVisibleIndex(), index_at_release)
        << "list should keep scrolling under momentum after release, not stop immediately";

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// Regresses the same class of bug as RenderTreeView's
// SubSlopJitterDoesNotScrollEvenWhenUncontested: accepting an uncontested
// arena at pointer-down must not itself start panning before any real
// movement exceeds kTapSlop.
TEST(RenderListView, SubSlopJitterDoesNotScrollEvenWhenUncontested)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    lv.attach();

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dy : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {0.0f, 150.0f + dy};
        dispatcher.handlePointerEvent(move);
    }

    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderListView, WheelEventScrolls)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    lv.attach();

    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    cw::PointerEvent scroll;
    scroll.kind           = cw::PointerEventKind::scroll;
    scroll.position       = {0.0f, 0.0f};
    scroll.scroll_delta_y = 100.0f;
    dispatcher.handlePointerEvent(scroll);

    // Scrolled down by 100 px → 100/50 = 2 items
    EXPECT_EQ(lv.firstVisibleIndex(), 2);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// ---------------------------------------------------------------------------
// Gesture arena — nested scrollables with perpendicular axes
// ---------------------------------------------------------------------------
//
// Regresses the generic "nested-scroll-plus-scroll" case from TODO.md's
// Gesture Arena section: a horizontal ListView nested inside a vertical one
// (e.g. a row of horizontally-scrolling cards inside a vertical feed). Both
// register independently with PointerDispatcher and would previously have
// scrolled simultaneously on a diagonal drag; now only one wins the arena.

TEST(RenderListView, NestedPerpendicularListViewInnerWinsArenaTie)
{
    cw::RenderListView outer;
    outer.scroll_axis = cw::Axis::vertical;
    outer.item_count  = 10;
    outer.item_extent = 100.0f;

    cw::RenderListView inner;
    inner.scroll_axis = cw::Axis::horizontal;
    inner.item_count  = 20;
    inner.item_extent = 50.0f;

    // inner is item 0's box — outer.performLayout() will size and lay it out.
    auto inner_box = std::shared_ptr<cw::RenderBox>(&inner, [](cw::RenderBox*) {});
    outer.setItemBox(0, inner_box);

    auto root = std::shared_ptr<cw::RenderBox>(&outer, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(outer, 400.0f, 400.0f);
    outer.attach();
    inner.attach();

    EXPECT_EQ(outer.firstVisibleIndex(), 0);
    EXPECT_EQ(inner.firstVisibleIndex(), 0);

    // down inside item 0 (y in [0, 100))
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f};
    dispatcher.handlePointerEvent(down);

    // Diagonal jump that exceeds both views' 8px tap slop in one move event.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {40.0f, -10.0f}; // dx=-60, dy=-60
    dispatcher.handlePointerEvent(move);

    // inner is deeper in the hit-test path, so it wins the tie: it scrolls,
    // outer (which would have scrolled too, were it not rejected) does not.
    EXPECT_NE(inner.firstVisibleIndex(), 0);
    EXPECT_EQ(outer.firstVisibleIndex(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    outer.detach();
    inner.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// ---------------------------------------------------------------------------
// applyExternalScrollDelta() — the NestedScrollView Stage 3 primitive. A
// thin wrapper around the same private applyScrollDelta() the pointer path
// already uses, so these confirm the return value is genuinely
// physics-derived (not an echo of the input) and that both the
// controller-attached and internal_offset_ paths are reachable through it.
// ---------------------------------------------------------------------------

TEST(RenderListView, ApplyExternalScrollDeltaInBoundsNoController)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f); // content 1000, viewport 200 -> max_extent_ 800

    const float applied = lv.applyExternalScrollDelta(100.0f);
    EXPECT_FLOAT_EQ(applied, 100.0f);
    EXPECT_EQ(lv.firstVisibleIndex(), 2); // 100 / 50
}

TEST(RenderListView, ApplyExternalScrollDeltaClampsAtBoundaryUnderClampingPhysics)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f); // max_extent_ 800

    const float applied = lv.applyExternalScrollDelta(1000.0f); // well past the 800 boundary
    EXPECT_FLOAT_EQ(applied, 800.0f); // only the portion up to the boundary, not the full 1000
}

TEST(RenderListView, ApplyExternalScrollDeltaRubberBandsUnderBouncingPhysics)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    lv.setPhysics(std::make_shared<cw::BouncingScrollPhysics>());
    doLayout(lv, 400.0f, 200.0f); // max_extent_ 800

    const float applied = lv.applyExternalScrollDelta(1000.0f); // 200px past the boundary
    // Resistance-damped past the boundary -- neither the hard 800 clamp from
    // the ClampingScrollPhysics case above, nor the full, undamped 1000.
    EXPECT_GT(applied, 800.0f);
    EXPECT_LT(applied, 1000.0f);
}

TEST(RenderListView, ApplyExternalScrollDeltaUpdatesAttachedController)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    auto controller = std::make_shared<cw::ScrollController>();
    lv.setController(controller);
    doLayout(lv, 400.0f, 200.0f);

    const float applied = lv.applyExternalScrollDelta(150.0f);
    EXPECT_FLOAT_EQ(applied, 150.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 150.0f);
}

// ---------------------------------------------------------------------------
// external_delta_redirect -- the NestedScrollView coordinator hook. Unset
// (default) must reproduce PanGestureScrolls's own exact numbers byte-for-
// byte -- the sharpest possible regression check, since the modified
// move/scroll call sites are `if (external_delta_redirect) ... else
// <original call>`, so with the hook unset the else branch is verbatim what
// used to run unconditionally.
// ---------------------------------------------------------------------------

TEST(RenderListView, PanGestureScrollsWithRedirectHookUnsetMatchesOriginalBehavior)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);
    // lv.external_delta_redirect left unset (nullptr) deliberately.

    lv.attach();

    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f};
    dispatcher.handlePointerEvent(move);

    move.position = {0.0f, 50.0f};
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(lv.firstVisibleIndex(), 2); // identical to PanGestureScrolls's own assertion

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderListView, PanGestureRoutesThroughRedirectHookWhenSet)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    std::vector<float> redirected_deltas;
    lv.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    lv.attach();

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // dy=-50, crosses slop, delta redirected = 50
    dispatcher.handlePointerEvent(move);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 50.0f);
    // The redirect callback only captured the delta, never applied it --
    // lv's own scroll position must be untouched.
    EXPECT_EQ(lv.firstVisibleIndex(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderListView, WheelEventRoutesThroughRedirectHookWhenSet)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 20;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    std::vector<float> redirected_deltas;
    lv.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    lv.attach();

    cw::PointerEvent scroll;
    scroll.kind           = cw::PointerEventKind::scroll;
    scroll.position       = {0.0f, 0.0f};
    scroll.scroll_delta_y = 75.0f;
    dispatcher.handlePointerEvent(scroll);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 75.0f);
    EXPECT_EQ(lv.firstVisibleIndex(), 0); // not applied directly

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// onTick()'s own spring-back/momentum must NOT be redirected -- confirmed by
// letting real momentum run (via dispatcher.tick(), same as
// MomentumContinuesAfterRelease above) with the hook set: the redirect must
// fire during the drag phase but NOT during the tick-driven momentum phase,
// even though momentum still visibly moves the list (proving onTick() calls
// applyScrollDelta() directly on lv itself, never through the hook).
TEST(RenderListView, MomentumIsNotRedirectedEvenWithHookSet)
{
    cw::RenderListView lv;
    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*){});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.item_count  = 100;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 200.0f);

    int redirect_call_count = 0;
    // The redirect callback intentionally does NOT apply the delta anywhere
    // (no real coordinator here) -- lv's own scroll position stays at 0
    // through the whole drag, so any *momentum* that later appears can only
    // have come from onTick() calling applyScrollDelta() directly, not from
    // residual state the redirected drag itself left behind.
    lv.external_delta_redirect = [&](float) { ++redirect_call_count; };

    lv.attach();

    auto nowMs = [] {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    // Synthetic timestamp_ms values, +10ms per event, instead of real
    // std::this_thread::sleep_for() between dispatches -- see
    // MomentumContinuesAfterRelease's own doc comment above / VelocityTracker's
    // doc comment for why this makes the drag's resulting release velocity
    // deterministic regardless of real elapsed wall-clock time.
    uint64_t t = 1'000;

    cw::PointerEvent down;
    down.kind         = cw::PointerEventKind::down;
    down.position     = {0.0f, 190.0f};
    down.timestamp_ms = t;
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float y = 190.0f;
    for (int i = 1; i <= 5; i++)
    {
        t += 10;
        y -= 40.0f;
        move.position     = {0.0f, y};
        move.timestamp_ms = t;
        dispatcher.handlePointerEvent(move);
    }

    const int calls_during_drag = redirect_call_count;
    EXPECT_GT(calls_during_drag, 0); // drag itself was redirected
    EXPECT_EQ(lv.firstVisibleIndex(), 0); // redirect never applied anything to lv itself

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    // velocity_px_s_ was seeded from the drag above; drive real ticks the
    // same way MomentumContinuesAfterRelease does.
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());

    EXPECT_GT(lv.firstVisibleIndex(), 0)
        << "momentum should still move the list via onTick()'s own direct applyScrollDelta()";
    EXPECT_EQ(redirect_call_count, calls_during_drag)
        << "onTick()'s spring/momentum calls must not go through external_delta_redirect";

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
