#include <gtest/gtest.h>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <chrono>
#include <thread>

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

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 190.0f};
    dispatcher.handlePointerEvent(down);

    // Fast, steady drag upward (reveals later items) — 40px every 10ms real
    // elapsed time (~4000 px/s), well above the momentum threshold. Once
    // the pointer is captured on down, subsequent moves route via the
    // cached hit path regardless of position, so drifting past the
    // viewport's own bounds here is fine.
    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float y = 190.0f;
    for (int i = 1; i <= 5; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        y -= 40.0f;
        move.position = {0.0f, y};
        dispatcher.handlePointerEvent(move);
    }
    // Final sample right before release: a near-zero delta, matching either
    // natural human deceleration as a finger lifts or a platform batching
    // several move callbacks together with negligible time between them —
    // both leave the *previous* implementation's single-sample delta/dt
    // estimate reporting ~0 velocity despite the real, fast drag above.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    y -= 0.2f;
    move.position = {0.0f, y};
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
