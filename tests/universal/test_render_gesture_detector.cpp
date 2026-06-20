#include <gtest/gtest.h>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

#include <chrono>

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
            det.on_pan_down   = [this](cw::Offset) { ++pan_down_count; };
            det.on_pan_update = [this](cw::Offset) { ++pan_update_count; };
            det.on_pan_end    = [this] { ++pan_end_count; };
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
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 1);
    EXPECT_EQ(pan_end_count, 0);
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
    move.position = {50.0f, 80.0f}; // 30px, exceeds 18px kTapSlop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(pan_update_count, 1);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 80.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(pan_end_count, 1);
    EXPECT_EQ(tap_count, 0);
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

    // down_time_ms_ is captured from a real steady_clock read, so the tick
    // timestamp must be on the same real-world timescale to avoid unsigned
    // underflow in the (now_ms - down_time_ms_) comparison.
    const auto now = std::chrono::steady_clock::now();
    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    dispatcher->tick(now_ms + 600); // past kLongPressMs (500ms)

    EXPECT_EQ(long_press_count, 1);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {50.0f, 50.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(tap_count, 0); // long press already claimed the gesture
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
    det.on_pan_update = [&](cw::Offset) { ++pan_update_count; };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {100.0f, 70.0f}; // 20px, exceeds the 18px GestureDetector slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GE(pan_update_count, 1);
    EXPECT_EQ(tap_count, 0);
    EXPECT_EQ(list.firstVisibleIndex(), 0); // list lost the arena, didn't scroll

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {100.0f, 70.0f};
    dispatcher->handlePointerEvent(up);
}
