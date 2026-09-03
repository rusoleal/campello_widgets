#include <gtest/gtest.h>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

#include <chrono>
#include <cmath>
#include <thread>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Standalone — tap / double-tap / long-press / pan
// ---------------------------------------------------------------------------

namespace
{
    // Wraps a RenderGestureDetector with its own dispatcher and tears both
    // down automatically, mirroring the pattern used for RenderListView/
    // RenderTreeView pointer tests.
    struct GestureDetectorFixture : public ::testing::Test
    {
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        int  tap_count        = 0;
        int  double_tap_count = 0;
        int  long_press_count = 0;
        int  pan_down_count   = 0;
        int  pan_update_count = 0;
        int  pan_end_count    = 0;
        cw::Offset last_pan_end_velocity;

        int  tap_down_count    = 0;
        int  tap_up_count      = 0;
        int  tap_cancel_count  = 0;

        int  secondary_tap_count        = 0;
        int  secondary_tap_down_count   = 0;
        int  secondary_tap_up_count     = 0;
        int  secondary_tap_cancel_count = 0;

        int  tertiary_tap_down_count    = 0;
        int  tertiary_tap_up_count      = 0;
        int  tertiary_tap_cancel_count  = 0;

        int  long_press_down_count        = 0;
        int  long_press_cancel_count      = 0;
        int  long_press_start_count       = 0;
        int  long_press_move_update_count = 0;
        int  long_press_end_count         = 0;
        cw::Offset last_long_press_end_velocity;

        void SetUp() override
        {
            auto root = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            det.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            det.attach();

            det.on_tap        = [this] { ++tap_count; };
            det.on_double_tap = [this] { ++double_tap_count; };
            det.on_long_press = [this] { ++long_press_count; };
            det.on_pan_down   = [this](cw::DragDownDetails) { ++pan_down_count; };
            det.on_pan_update = [this](cw::DragUpdateDetails) { ++pan_update_count; };
            det.on_pan_end    = [this](cw::DragEndDetails d) { ++pan_end_count; last_pan_end_velocity = d.velocity; };

            det.on_tap_down   = [this](cw::TapDownDetails) { ++tap_down_count; };
            det.on_tap_up     = [this](cw::TapUpDetails) { ++tap_up_count; };
            det.on_tap_cancel = [this] { ++tap_cancel_count; };

            det.on_secondary_tap        = [this] { ++secondary_tap_count; };
            det.on_secondary_tap_down   = [this](cw::TapDownDetails) { ++secondary_tap_down_count; };
            det.on_secondary_tap_up     = [this](cw::TapUpDetails) { ++secondary_tap_up_count; };
            det.on_secondary_tap_cancel = [this] { ++secondary_tap_cancel_count; };

            det.on_tertiary_tap_down   = [this](cw::TapDownDetails) { ++tertiary_tap_down_count; };
            det.on_tertiary_tap_up     = [this](cw::TapUpDetails) { ++tertiary_tap_up_count; };
            det.on_tertiary_tap_cancel = [this] { ++tertiary_tap_cancel_count; };

            det.on_long_press_down = [this](cw::LongPressDownDetails) { ++long_press_down_count; };
            det.on_long_press_cancel = [this] { ++long_press_cancel_count; };
            det.on_long_press_start  = [this](cw::LongPressStartDetails) { ++long_press_start_count; };
            det.on_long_press_move_update =
                [this](cw::LongPressMoveUpdateDetails) { ++long_press_move_update_count; };
            det.on_long_press_end = [this](cw::LongPressEndDetails d) {
                ++long_press_end_count;
                last_long_press_end_velocity = d.velocity;
            };
        }

        void TearDown() override
        {
            det.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(GestureDetectorFixture, TapFiresOnQuickPressRelease)
{
    EXPECT_EQ(tap_down_count, 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(tap_down_count, 1); // fires eagerly, before up
    EXPECT_EQ(tap_up_count, 0);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 1);
    EXPECT_EQ(tap_up_count, 1);
    EXPECT_EQ(tap_cancel_count, 0);
    EXPECT_EQ(pan_end_count, 0);
}

TEST_F(GestureDetectorFixture, SecondaryTapFiresOnlySecondaryCallbacksNotPrimary)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    down.button   = cw::PointerButton::secondary;
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(secondary_tap_down_count, 1);
    EXPECT_EQ(tap_down_count, 0);
    EXPECT_EQ(tertiary_tap_down_count, 0);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    up.button   = cw::PointerButton::secondary;
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(secondary_tap_count, 1);
    EXPECT_EQ(secondary_tap_up_count, 1);
    EXPECT_EQ(secondary_tap_cancel_count, 0);
    EXPECT_EQ(tap_count, 0);
    EXPECT_EQ(tertiary_tap_up_count, 0);
}

TEST_F(GestureDetectorFixture, TertiaryTapFiresOnlyTertiaryCallbacksAndHasNoPlainTap)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    down.button   = cw::PointerButton::tertiary;
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(tertiary_tap_down_count, 1);
    EXPECT_EQ(tap_down_count, 0);
    EXPECT_EQ(secondary_tap_down_count, 0);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    up.button   = cw::PointerButton::tertiary;
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tertiary_tap_up_count, 1);
    // Flutter has no plain "onTertiaryTap" -- only down/up/cancel.
    EXPECT_EQ(tap_count, 0);
    EXPECT_EQ(secondary_tap_count, 0);
}

TEST_F(GestureDetectorFixture, DefaultButtonTapStillFiresOnlyPrimaryCallbacks)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    // button left at its default (primary).
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 1);
    EXPECT_EQ(secondary_tap_count, 0);
    EXPECT_EQ(secondary_tap_down_count, 0);
    EXPECT_EQ(tertiary_tap_down_count, 0);
}

TEST_F(GestureDetectorFixture, OnPanDownFiresImmediatelyOnDown)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(pan_down_count, 1);
    EXPECT_EQ(tap_count, 0); // not resolved yet
}

TEST_F(GestureDetectorFixture, PanFiresUpdateAndEndInsteadOfTap)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 100.0f}; // 50px, exceeds the 36px (touch) pan slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(pan_update_count, 1);
    EXPECT_EQ(tap_cancel_count, 1); // slop exceeded -- pending tap abandoned

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 100.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(pan_end_count, 1);
    EXPECT_EQ(tap_count, 0);
    EXPECT_EQ(tap_up_count, 0);
    EXPECT_EQ(tap_cancel_count, 1); // still just the one cancel, not fired again on up
}

TEST_F(GestureDetectorFixture, DoubleTapFiresOnSecondQuickTap)
{
    cw::PointerEvent down1;
    down1.kind     = cw::PointerEventKind::down;
    down1.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down1);

    cw::PointerEvent up1;
    up1.kind     = cw::PointerEventKind::up;
    up1.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up1);

    cw::PointerEvent down2;
    down2.kind     = cw::PointerEventKind::down;
    down2.position = {52.0f, 51.0f};
    dispatcher->handlePointerEvent(down2);

    cw::PointerEvent up2;
    up2.kind     = cw::PointerEventKind::up;
    up2.position = {52.0f, 51.0f};
    dispatcher->handlePointerEvent(up2);

    EXPECT_EQ(double_tap_count, 1);
    EXPECT_EQ(tap_count, 1); // only the first tap fires on_tap
}

TEST_F(GestureDetectorFixture, LongPressFiresAfterDelayWithoutMoving)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(long_press_down_count, 1); // fires eagerly, before the deadline
    EXPECT_EQ(long_press_start_count, 0);

    // down_time_ms_ is captured from a real steady_clock read, so the tick
    // timestamp must be on the same real-world timescale to avoid unsigned
    // underflow in the (now_ms - down_time_ms_) comparison.
    const auto now = std::chrono::steady_clock::now();
    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    dispatcher->tick(now_ms + 600); // past kLongPressMs (500ms)

    EXPECT_EQ(long_press_count, 1);
    EXPECT_EQ(long_press_start_count, 1);
    EXPECT_EQ(long_press_cancel_count, 0);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 0); // long press already claimed the gesture
    EXPECT_EQ(long_press_end_count, 1);
}

TEST_F(GestureDetectorFixture, LongPressCancelFiresWhenMovementPreemptsTheDeadline)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {80.0f, 50.0f}; // 30px, exceeds kStationaryTolerance (18px)
    dispatcher->handlePointerEvent(move);

    EXPECT_EQ(long_press_cancel_count, 1);
    EXPECT_EQ(long_press_start_count, 0);

    const auto now = std::chrono::steady_clock::now();
    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    dispatcher->tick(now_ms + 600); // would have fired if not already cancelled

    EXPECT_EQ(long_press_count, 0);
    EXPECT_EQ(long_press_start_count, 0);
    EXPECT_EQ(long_press_cancel_count, 1); // not fired again
}

TEST_F(GestureDetectorFixture, LongPressMoveUpdateFiresAfterStartAndEndCarriesVelocity)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    const auto now = std::chrono::steady_clock::now();
    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    dispatcher->tick(now_ms + 600); // past kLongPressMs -- press has started

    ASSERT_EQ(long_press_start_count, 1);

    // Movement after the press has started no longer threatens/cancels the
    // gesture (see LongPressGestureRecognizer's doc comment) -- it's
    // tracked and reported instead.
    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float x = 50.0f;
    for (int i = 1; i <= 5; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        x += 40.0f;
        move.position = {x, 50.0f};
        dispatcher->handlePointerEvent(move);
    }

    EXPECT_GE(long_press_move_update_count, 1);
    EXPECT_EQ(long_press_cancel_count, 0); // still not cancelled

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {x, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(long_press_end_count, 1);
    EXPECT_GT(last_long_press_end_velocity.x, 0.0f)
        << "steady rightward drag after the press started should report positive-x velocity";
}

// ---------------------------------------------------------------------------
// Gesture arena — GestureDetector nested inside a pannable ancestor
// ---------------------------------------------------------------------------
//
// Regresses the "nested-pan-plus-tap" case from TODO.md's Gesture Arena
// section: a tappable widget (e.g. a button) inside a scrollable list used
// to receive the identical pointer stream as the list's own pan-to-scroll,
// with no arbitration between them.

namespace
{
    struct NestedGestureFixture : public ::testing::Test
    {
        cw::RenderListView list;
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        int tap_count = 0;

        void SetUp() override
        {
            list.scroll_axis = cw::Axis::vertical;
            list.item_count  = 10;
            list.item_extent = 100.0f;

            auto det_box = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            list.setItemBox(0, det_box);

            auto root = std::shared_ptr<cw::RenderBox>(&list, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            list.layout(cw::BoxConstraints::tight(400.0f, 400.0f));
            list.attach();
            det.attach();

            det.on_tap = [this] { ++tap_count; };
        }

        void TearDown() override
        {
            det.detach();
            list.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(NestedGestureFixture, DragThroughButtonScrollsListAndSuppressesTap)
{
    EXPECT_EQ(list.firstVisibleIndex(), 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f}; // inside item 0 / the button
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {100.0f, -50.0f}; // 100px up, exceeds list's 8px slop
    dispatcher->handlePointerEvent(move);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {100.0f, -50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 0);                 // suppressed — list claimed the gesture
    EXPECT_NE(list.firstVisibleIndex(), 0);  // list actually scrolled
}

TEST_F(NestedGestureFixture, QuickTapOnButtonFiresAndDoesNotScrollList)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {100.0f, 50.0f}; // no movement
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 1);
    EXPECT_EQ(list.firstVisibleIndex(), 0);
}

// A GestureDetector that actually wants pan callbacks (e.g. a custom
// drag-handle widget, not just a tappable button) legitimately competes for
// the gesture rather than always deferring to an ancestor scrollable.
TEST_F(NestedGestureFixture, PanEnabledButtonStillFiresOwnPanUpdate)
{
    int pan_update_count = 0;
    det.on_pan_update = [&](cw::DragUpdateDetails) { ++pan_update_count; };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {100.0f, 110.0f}; // 60px, exceeds the 36px (touch) pan slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(pan_update_count, 1);
    EXPECT_EQ(tap_count, 0);
    EXPECT_EQ(list.firstVisibleIndex(), 0); // list lost the arena, didn't scroll

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {100.0f, 110.0f};
    dispatcher->handlePointerEvent(up);
}

// ---------------------------------------------------------------------------
// Wave 2 — axis-locked horizontal/vertical drag, velocity on pan end
// ---------------------------------------------------------------------------

namespace
{
    // on_horizontal_drag_* only -- on_pan_* stays unset, since the two
    // families are mutually exclusive (debug-asserted in
    // RenderGestureDetector::onPointerEvent).
    struct HorizontalDragFixture : public ::testing::Test
    {
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        int horizontal_update_count = 0;

        void SetUp() override
        {
            auto root = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            det.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            det.attach();

            det.on_horizontal_drag_update = [this](cw::DragUpdateDetails) { ++horizontal_update_count; };
        }

        void TearDown() override
        {
            det.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };

    // on_vertical_drag_* only -- same mutual-exclusion reasoning.
    struct VerticalDragFixture : public ::testing::Test
    {
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        int vertical_update_count = 0;

        void SetUp() override
        {
            auto root = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            det.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            det.attach();

            det.on_vertical_drag_update = [this](cw::DragUpdateDetails) { ++vertical_update_count; };
        }

        void TearDown() override
        {
            det.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(HorizontalDragFixture, IgnoresPureVerticalMovement)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 100.0f}; // 50px vertical, 0px horizontal -- never exceeds horizontal slop
    dispatcher->handlePointerEvent(move);

    EXPECT_EQ(horizontal_update_count, 0);
}

TEST_F(HorizontalDragFixture, FiresOnHorizontalMovement)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {110.0f, 50.0f}; // 60px horizontal, exceeds the 36px (touch) pan slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(horizontal_update_count, 1);
}

TEST_F(VerticalDragFixture, IgnoresPureHorizontalMovement)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {110.0f, 50.0f}; // 60px horizontal, 0px vertical -- never exceeds vertical slop
    dispatcher->handlePointerEvent(move);

    EXPECT_EQ(vertical_update_count, 0);
}

TEST_F(VerticalDragFixture, FiresOnVerticalMovement)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 110.0f}; // 60px vertical, exceeds the 36px (touch) pan slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(vertical_update_count, 1);
}

// Mirrors RenderListView's MomentumContinuesAfterRelease test's rationale: a
// naive last-two-samples delta/dt estimate would report ~0 velocity from the
// final, near-stationary sample right before release, even though the drag
// was fast throughout -- VelocityTracker's trailing-window least-squares fit
// should still report real, non-trivial velocity. Uses synthetic
// timestamp_ms values instead of real std::this_thread::sleep_for() between
// dispatches, so the intended ~4000 px/s drag is deterministic regardless of
// how long this process actually takes to execute each line (see
// VelocityTracker's own doc comment).
TEST_F(GestureDetectorFixture, PanEndCarriesReleaseVelocity)
{
    uint64_t t = 1'000;

    // Down must land inside the fixture's 200x100 box for the initial
    // hit-test to capture the pointer -- subsequent moves then route via
    // the captured path regardless of position (see PointerDispatcher's
    // move-handling comment), so they're free to drift outside it.
    cw::PointerEvent down;
    down.kind         = cw::PointerEventKind::down;
    down.position     = {50.0f, 90.0f};
    down.timestamp_ms = t;
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float y = 90.0f;
    for (int i = 1; i <= 5; i++)
    {
        t += 10;
        y -= 40.0f; // fast, steady drag upward: ~4000 px/s
        move.position     = {50.0f, y};
        move.timestamp_ms = t;
        dispatcher->handlePointerEvent(move);
    }
    // Near-zero final delta right before release.
    t += 10;
    y -= 0.2f;
    move.position     = {50.0f, y};
    move.timestamp_ms = t;
    dispatcher->handlePointerEvent(move);

    cw::PointerEvent up;
    up.kind         = cw::PointerEventKind::up;
    up.position     = {50.0f, y};
    up.timestamp_ms = t;
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(pan_end_count, 1);
    EXPECT_LT(last_pan_end_velocity.y, -500.0f)
        << "fast upward drag should report substantial negative-y (upward) velocity, not ~0";
}

// ---------------------------------------------------------------------------
// Wave 4 — HitTestBehavior (opaque / deferToChild / translucent)
// ---------------------------------------------------------------------------

namespace
{
    // Outer detector wraps an inner one as its single child, so a hit can
    // land either on the inner child's bounds or only on the outer's.
    struct DeferToChildFixture : public ::testing::Test
    {
        cw::RenderGestureDetector outer;
        cw::RenderGestureDetector inner;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        int outer_tap_count = 0;
        int inner_tap_count = 0;

        void SetUp() override
        {
            auto inner_box = std::shared_ptr<cw::RenderBox>(&inner, [](cw::RenderBox*) {});
            outer.setChild(inner_box);

            auto root = std::shared_ptr<cw::RenderBox>(&outer, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            // Outer is 200x100; inner is tight to a 50x50 region at (0,0),
            // leaving the rest of outer's bounds uncovered by any child.
            outer.behavior = cw::HitTestBehavior::deferToChild;
            outer.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            inner.layout(cw::BoxConstraints::tight(50.0f, 50.0f));
            outer.attach();
            inner.attach();

            outer.on_tap = [this] { ++outer_tap_count; };
            inner.on_tap = [this] { ++inner_tap_count; };
        }

        void TearDown() override
        {
            inner.detach();
            outer.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };

    // Two same-sized detectors stacked at the same point: back at index 0
    // (painted first, underneath), front at index 1 (painted last, on top).
    // RenderStack::hitTestChildren() walks back-to-front (rbegin), so front
    // is hit-tested first.
    struct StackedGestureFixture : public ::testing::Test
    {
        cw::RenderStack stack;
        cw::RenderGestureDetector front;
        cw::RenderGestureDetector back;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        int front_tap_count = 0;
        int back_tap_count  = 0;

        void SetUp() override
        {
            auto front_box = std::shared_ptr<cw::RenderBox>(&front, [](cw::RenderBox*) {});
            auto back_box  = std::shared_ptr<cw::RenderBox>(&back,  [](cw::RenderBox*) {});
            // Explicit left/top/width/height makes both "positioned" so
            // they get a concrete, fully-overlapping 200x100 size — a
            // non-positioned StackFit::loose child would collapse to 0x0
            // with no child of its own to size from.
            stack.insertChild(back_box,  0, 0.0f, 0.0f, std::nullopt, std::nullopt, 200.0f, 100.0f);
            stack.insertChild(front_box, 1, 0.0f, 0.0f, std::nullopt, std::nullopt, 200.0f, 100.0f);

            auto root = std::shared_ptr<cw::RenderBox>(&stack, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            stack.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            stack.attach();
            front.attach();
            back.attach();

            front.on_tap = [this] { ++front_tap_count; };
            back.on_tap  = [this] { ++back_tap_count; };
        }

        void TearDown() override
        {
            front.detach();
            back.detach();
            stack.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(DeferToChildFixture, OuterDoesNotFireWhenHitLandsOnInnerChild)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {25.0f, 25.0f}; // inside inner's 50x50 region
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {25.0f, 25.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(inner_tap_count, 1);
    EXPECT_EQ(outer_tap_count, 0)
        << "deferToChild: outer must not join the dispatch/arena at all once a child claimed the point";
}

TEST_F(DeferToChildFixture, OuterFiresWhenHitLandsOutsideInnerChild)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {150.0f, 75.0f}; // outside inner's 50x50 region, inside outer's 200x100
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {150.0f, 75.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(inner_tap_count, 0);
    EXPECT_EQ(outer_tap_count, 1);
}

TEST_F(StackedGestureFixture, OpaqueFrontBlocksBackFromEverBeingHitTested)
{
    // opaque is the default -- regression check that stacking behavior is
    // unchanged from before HitTestBehavior existed.
    int back_tap_down_count = 0;
    back.on_tap_down = [&](cw::TapDownDetails) { ++back_tap_down_count; };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);
    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(front_tap_count, 1);
    EXPECT_EQ(back_tap_count, 0);
    EXPECT_EQ(back_tap_down_count, 0)
        << "back must never even be hit-tested/dispatched to -- front (opaque) stops the Stack's walk";
}

TEST_F(StackedGestureFixture, TranslucentFrontLetsBackAlsoBeHitTestedAndDispatchedTo)
{
    front.behavior = cw::HitTestBehavior::translucent;

    int back_tap_down_count = 0;
    back.on_tap_down = [&](cw::TapDownDetails) { ++back_tap_down_count; };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    EXPECT_EQ(back_tap_down_count, 1)
        << "translucent: RenderStack must keep testing lower-painted siblings, so back is reached too";
}

// Documents a real, non-obvious interaction: translucent controls hit-test
// *reachability* (whether a sibling further back in the Stack is tested and
// dispatched to at all), not gesture-arena independence. Two overlapping
// TapGestureRecognizers for the *same* pointer still compete in the *same*
// arena -- front (hit-tested first, so added to the arena first) wins the
// default sweep, same as real Flutter. translucent alone doesn't make both
// simultaneously "win" a tap.
TEST_F(StackedGestureFixture, TranslucentSiblingsStillCompeteInTheSameArenaForTap)
{
    front.behavior = cw::HitTestBehavior::translucent;

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);
    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(front_tap_count, 1);
    EXPECT_EQ(back_tap_count, 0);
}

// ---------------------------------------------------------------------------
// Wave 5 — ScaleGestureRecognizer (pinch / rotate / multi-pointer)
// ---------------------------------------------------------------------------

namespace
{
    struct ScaleGestureFixture : public ::testing::Test
    {
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        int    scale_start_count  = 0;
        int    scale_update_count = 0;
        int    scale_end_count    = 0;
        float  last_scale         = 1.0f;
        float  last_h_scale       = 1.0f;
        float  last_v_scale       = 1.0f;
        float  last_rotation      = 0.0f;
        int    last_pointer_count = 0;
        cw::Offset last_focal;
        cw::Offset last_end_velocity;

        void SetUp() override
        {
            auto root = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            det.layout(cw::BoxConstraints::tight(400.0f, 400.0f));
            det.attach();

            det.on_scale_start = [this](cw::ScaleStartDetails) { ++scale_start_count; };
            det.on_scale_update = [this](cw::ScaleUpdateDetails d) {
                ++scale_update_count;
                last_scale         = d.scale;
                last_h_scale       = d.horizontal_scale;
                last_v_scale       = d.vertical_scale;
                last_rotation      = d.rotation;
                last_pointer_count = d.pointer_count;
                last_focal         = d.focal_point;
            };
            det.on_scale_end = [this](cw::ScaleEndDetails d) {
                ++scale_end_count;
                last_end_velocity = d.velocity;
            };
        }

        void TearDown() override
        {
            det.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }

        static cw::PointerEvent makeEvent(cw::PointerEventKind kind, int32_t pointer_id, cw::Offset pos)
        {
            cw::PointerEvent e;
            e.kind       = kind;
            e.pointer_id = pointer_id;
            e.position   = pos;
            return e;
        }
    };
} // namespace

TEST_F(ScaleGestureFixture, TwoFingerPinchScalesBothDirections)
{
    // Two pointers 100px apart horizontally, centered in the box.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 0, {150.0f, 200.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 1, {250.0f, 200.0f}));

    EXPECT_EQ(scale_start_count, 1)
        << "a second concurrent pointer is unambiguous multi-touch -- starts immediately, no slop wait";

    // Spread apart to 200px apart (2x) -- scale should roughly double.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 0, {100.0f, 200.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 1, {300.0f, 200.0f}));

    EXPECT_GE(scale_update_count, 1);
    EXPECT_GT(last_scale, 1.8f) << "spreading two fingers apart should roughly double the reported scale";
    EXPECT_EQ(last_pointer_count, 2);

    // Now bring them together, closer than the original 100px span.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 0, {180.0f, 200.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 1, {220.0f, 200.0f}));

    EXPECT_LT(last_scale, 1.0f) << "pinching fingers together should bring scale back below 1.0";

    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::up, 0, {180.0f, 200.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::up, 1, {220.0f, 200.0f}));

    EXPECT_EQ(scale_end_count, 1) << "end fires once the last of the two pointers lifts";
}

TEST_F(ScaleGestureFixture, TwoFingerRotationProducesNonZeroRotation)
{
    // Two pointers on a vertical line through the centroid (200, 200),
    // 100px from center each.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 0, {200.0f, 100.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 1, {200.0f, 300.0f}));
    ASSERT_EQ(scale_start_count, 1);

    // Rotate the line 90 degrees (to horizontal) in small steps, well under
    // the recognizer's +/-45 degree per-frame unwrap threshold.
    constexpr float kRadius = 100.0f;
    constexpr float kCx = 200.0f, kCy = 200.0f;
    for (int step = 1; step <= 6; ++step)
    {
        const float theta = (3.14159265f / 2.0f) * (static_cast<float>(step) / 6.0f); // 0..pi/2
        const float dx = kRadius * std::sin(theta);
        const float dy = kRadius * std::cos(theta);
        dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 0, {kCx - dx, kCy - dy}));
        dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 1, {kCx + dx, kCy + dy}));
    }

    EXPECT_GT(std::abs(last_rotation), 1.0f)
        << "rotating a two-finger line by 90 degrees should report substantial accumulated rotation";
}

TEST_F(ScaleGestureFixture, SingleFingerScaleDegradesToPanWithoutChangingScale)
{
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 0, {200.0f, 200.0f}));
    EXPECT_EQ(scale_start_count, 0) << "a single pointer must exceed slop before starting, like a plain pan";

    // 60px, exceeds the 36px (touch) pan slop.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 0, {260.0f, 200.0f}));

    EXPECT_EQ(scale_start_count, 1);
    EXPECT_GE(scale_update_count, 1);
    EXPECT_NEAR(last_scale, 1.0f, 0.001f)
        << "span-from-centroid is undefined for one point -- scale must stay pinned at identity";
    EXPECT_NEAR(last_rotation, 0.0f, 0.001f);
    EXPECT_EQ(last_pointer_count, 1);
    EXPECT_NEAR(last_focal.x, 260.0f, 0.5f) << "focal point should track the single pointer, like a pan";
}

TEST_F(ScaleGestureFixture, PointerCountTransitionKeepsScaleContinuous)
{
    // Start a single-finger scale (degrades to pan).
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 0, {200.0f, 200.0f}));
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 0, {260.0f, 200.0f}));
    ASSERT_EQ(scale_start_count, 1);
    ASSERT_NEAR(last_scale, 1.0f, 0.001f);
    const int updates_before_second_finger = scale_update_count;

    // A second finger joins mid-gesture.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::down, 1, {300.0f, 200.0f}));

    EXPECT_EQ(scale_start_count, 1) << "on_scale_start must not fire again for the joining pointer";
    EXPECT_EQ(scale_update_count, updates_before_second_finger)
        << "the join itself rebases silently -- it does not fire an update on its own";
    EXPECT_NEAR(last_scale, 1.0f, 0.001f) << "scale must not jump the instant the second pointer is added";

    // A small move afterwards should stay continuous (no large jump from the
    // freshly-rebased baseline), not snap to some unrelated ratio.
    dispatcher->handlePointerEvent(makeEvent(cw::PointerEventKind::move, 1, {310.0f, 200.0f}));

    EXPECT_GT(scale_update_count, updates_before_second_finger);
    EXPECT_NEAR(last_scale, 1.0f, 0.3f)
        << "scale should still be close to identity right after the transition, not discontinuous";
    EXPECT_EQ(last_pointer_count, 2);
}

// ---------------------------------------------------------------------------
// Wave 6 — ForcePressGestureRecognizer (stylus staged pressure)
// ---------------------------------------------------------------------------

namespace
{
    struct ForcePressFixture : public ::testing::Test
    {
        cw::RenderGestureDetector det;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        int   force_start_count  = 0;
        int   force_peak_count   = 0;
        int   force_update_count = 0;
        int   force_end_count    = 0;
        int   tap_count          = 0;
        float last_pressure      = 0.0f;

        void SetUp() override
        {
            auto root = std::shared_ptr<cw::RenderBox>(&det, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            det.layout(cw::BoxConstraints::tight(200.0f, 100.0f));
            det.attach();

            det.on_force_press_start  = [this](cw::ForcePressDetails d) { ++force_start_count; last_pressure = d.pressure; };
            det.on_force_press_peak   = [this](cw::ForcePressDetails) { ++force_peak_count; };
            det.on_force_press_update = [this](cw::ForcePressDetails d) { ++force_update_count; last_pressure = d.pressure; };
            det.on_force_press_end    = [this](cw::ForcePressDetails) { ++force_end_count; };
            det.on_tap                = [this] { ++tap_count; };
        }

        void TearDown() override
        {
            det.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }

        static cw::PointerEvent makeEvent(cw::PointerEventKind kind, cw::Offset pos, float pressure,
                                           cw::PointerDeviceKind device_kind)
        {
            cw::PointerEvent e;
            e.kind        = kind;
            e.position    = pos;
            e.pressure    = pressure;
            e.device_kind = device_kind;
            return e;
        }
    };
} // namespace

TEST_F(ForcePressFixture, StylusPressureRampFiresStartThenPeakThenEnd)
{
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::down, {50.0f, 50.0f}, 0.1f, cw::PointerDeviceKind::stylus));
    EXPECT_EQ(force_start_count, 0);

    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::move, {50.0f, 50.0f}, 0.5f, cw::PointerDeviceKind::stylus));
    EXPECT_EQ(force_start_count, 1) << "pressure crossed the 0.4 default start threshold";
    EXPECT_EQ(force_peak_count, 0);
    EXPECT_EQ(force_update_count, 0) << "update excludes the sample that just fired start";

    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::move, {50.0f, 50.0f}, 0.6f, cw::PointerDeviceKind::stylus));
    EXPECT_EQ(force_update_count, 1) << "a later sample below peak reports as a plain update";

    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::move, {50.0f, 50.0f}, 0.9f, cw::PointerDeviceKind::stylus));
    EXPECT_EQ(force_peak_count, 1) << "pressure crossed the 0.85 default peak threshold";

    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::up, {50.0f, 50.0f}, 0.9f, cw::PointerDeviceKind::stylus));
    EXPECT_EQ(force_end_count, 1);
}

TEST_F(ForcePressFixture, MousePressureNeverFiresForcePress)
{
    // Mouse events report a constant pressure=1.0 in this codebase, which
    // would trip both thresholds instantly if device-kind gating didn't
    // exclude non-stylus pointers entirely.
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::down, {50.0f, 50.0f}, 1.0f, cw::PointerDeviceKind::mouse));
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::up, {50.0f, 50.0f}, 1.0f, cw::PointerDeviceKind::mouse));

    EXPECT_EQ(force_start_count, 0);
    EXPECT_EQ(force_peak_count, 0);
    EXPECT_EQ(force_end_count, 0);
    EXPECT_EQ(tap_count, 1) << "regular tap recognition is unaffected";
}

TEST_F(ForcePressFixture, TouchPressureNeverFiresForcePress)
{
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::down, {50.0f, 50.0f}, 1.0f, cw::PointerDeviceKind::touch));
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::up, {50.0f, 50.0f}, 1.0f, cw::PointerDeviceKind::touch));

    EXPECT_EQ(force_start_count, 0);
    EXPECT_EQ(tap_count, 1);
}

// Force-press deliberately never joins the gesture arena (see
// ForcePressGestureRecognizer's doc comment) specifically so it coexists
// with tap/drag instead of competing with them for the win.
TEST_F(ForcePressFixture, StylusTapThatCrossesThresholdsStillFiresBothTapAndForcePress)
{
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::down, {50.0f, 50.0f}, 0.1f, cw::PointerDeviceKind::stylus));
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::move, {50.0f, 50.0f}, 0.9f, cw::PointerDeviceKind::stylus));
    dispatcher->handlePointerEvent(
        makeEvent(cw::PointerEventKind::up, {50.0f, 50.0f}, 0.9f, cw::PointerDeviceKind::stylus));

    EXPECT_EQ(force_start_count, 1);
    EXPECT_EQ(force_peak_count, 1);
    EXPECT_EQ(force_end_count, 1);
    EXPECT_EQ(tap_count, 1) << "force-press must not steal the arena win from the tap recognizer";
}
