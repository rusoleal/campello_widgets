#include <gtest/gtest.h>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/widgets/refresh_indicator.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <chrono>
#include <thread>
#include <vector>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// RefreshIndicator plan Wave 1 — ScrollController's overscroll signal.
//
// Note on scope: RenderGridView/RenderPageView/RenderTreeView deliberately do
// NOT get this wiring (see the plan) — confirmed by grep that
// ScrollController::notifyOverscroll() is only ever called from
// RenderSingleChildScrollView and RenderListView, so a RefreshIndicator
// wrapping one of those other scroll types simply never observes a nonzero
// pull, rather than crashing or behaving unexpectedly.
// ---------------------------------------------------------------------------

namespace
{
    uint64_t nowMs()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    struct ScrollViewOverscrollFixture : public ::testing::Test
    {
        cw::RenderSingleChildScrollView sv;
        std::shared_ptr<cw::ScrollController> controller = std::make_shared<cw::ScrollController>();
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        void SetUp() override
        {
            sv.scroll_axis = cw::Axis::vertical;
            sv.setController(controller);

            auto child = std::make_shared<cw::RenderSizedBox>();
            child->width  = 400.0f;
            child->height = 2000.0f; // plenty of scroll room
            sv.setChild(child);

            auto root = std::shared_ptr<cw::RenderBox>(&sv, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            sv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
            sv.attach();
        }

        void TearDown() override
        {
            sv.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(ScrollViewOverscrollFixture, DraggingPastTopFiresRawUnresistedMagnitude)
{
    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    // Drag DOWN (increasing y) at the top of the content — the pull-to-refresh
    // gesture. Content is already at offset 0, so any downward drag is an
    // immediate top overscroll.
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 150.0f}; // 100px down, exceeds slop
    dispatcher->handlePointerEvent(move);

    ASSERT_FALSE(reported.empty());
    // ClampingScrollPhysics (the default) hard-clamps the *displayed* offset
    // at 0 the whole time, but the raw pull distance must still be reported
    // in full — this is the entire point of the new channel.
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);
    EXPECT_NEAR(reported.back(), 100.0f, 0.5f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

TEST_F(ScrollViewOverscrollFixture, ScrollingWithinBoundsNeverFiresNonzero)
{
    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    // Drag UP — scrolls forward into content, well within bounds.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f};
    dispatcher->handlePointerEvent(move);

    EXPECT_GT(controller->offset(), 0.0f);
    for (float v : reported)
        EXPECT_FLOAT_EQ(v, 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

TEST_F(ScrollViewOverscrollFixture, ReleaseSpringBackEventuallySettlesOverscrollToZero)
{
    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 150.0f}; // pull down 100px
    dispatcher->handlePointerEvent(move);

    ASSERT_GT(reported.back(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);

    // Drive enough ticks for the spring-back easing (onTick()) to settle —
    // it eases raw_offset_ back toward the boundary and re-applies via the
    // same applyScrollDelta() path, so this also proves the settle phase
    // notifies, not just active dragging.
    for (int i = 0; i < 40; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        dispatcher->tick(nowMs());
    }

    ASSERT_FALSE(reported.empty());
    EXPECT_NEAR(reported.back(), 0.0f, 0.5f);
}

TEST_F(ScrollViewOverscrollFixture, BottomOverscrollNeverReportsNonzero)
{
    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    // Down must land inside the 200px-tall viewport for the initial hit-test
    // to capture the pointer at all (only subsequent MOVE positions are free
    // to drift outside it once captured). Then drag UP a single very large
    // distance — enough to both exceed slop and scroll straight past the
    // bottom boundary (max_extent_ = 2000 - 200 = 1800px) in one gesture.
    // ScrollController::jumpTo() only sets the controller's own displayed
    // offset_, not the render object's private raw_offset_, so a real drag
    // is required to actually move raw_offset_ near the bottom the way a
    // real gesture would.
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, -1950.0f}; // 2100px up in one move — past the bottom
    dispatcher->handlePointerEvent(move);

    EXPECT_FLOAT_EQ(controller->offset(), controller->maxScrollExtent());
    ASSERT_FALSE(reported.empty());
    for (float v : reported)
        EXPECT_FLOAT_EQ(v, 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

// ---------------------------------------------------------------------------
// Same coverage for RenderListView.
// ---------------------------------------------------------------------------

TEST(RefreshIndicatorOverscroll, ListViewDraggingPastTopFiresRawUnresistedMagnitude)
{
    cw::RenderListView lv;
    auto controller = std::make_shared<cw::ScrollController>();
    lv.setController(controller);
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    lv.attach();

    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 50.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 150.0f}; // pull down 100px
    dispatcher.handlePointerEvent(move);

    ASSERT_FALSE(reported.empty());
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);
    EXPECT_NEAR(reported.back(), 100.0f, 0.5f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RefreshIndicatorOverscroll, ListViewScrollingWithinBoundsNeverFiresNonzero)
{
    cw::RenderListView lv;
    auto controller = std::make_shared<cw::ScrollController>();
    lv.setController(controller);
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    auto root = std::shared_ptr<cw::RenderBox>(&lv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    lv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    lv.attach();

    std::vector<float> reported;
    controller->addOverscrollListener([&](float v) { reported.push_back(v); });

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // drag up — scrolls forward, in bounds
    dispatcher.handlePointerEvent(move);

    EXPECT_GT(controller->offset(), 0.0f);
    for (float v : reported)
        EXPECT_FLOAT_EQ(v, 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    lv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// ---------------------------------------------------------------------------
// Wave 3 — the RefreshIndicator widget's own state machine.
//
// These drive the widget purely through ScrollController::notifyOverscroll()
// directly (bypassing real pointer events / a mounted scroll render object
// entirely) — Wave 1's tests above already prove the render objects report
// the correct raw magnitude; this section only exercises
// RefreshIndicatorState's own pull/release/refresh/settle logic.
// ---------------------------------------------------------------------------

namespace
{
    struct RefreshIndicatorFixture : public ::testing::Test
    {
        std::shared_ptr<cw::ScrollController> controller = std::make_shared<cw::ScrollController>();
        std::shared_ptr<cw::RefreshIndicator> widget      = std::make_shared<cw::RefreshIndicator>();
        std::shared_ptr<cw::Element>          root_element;
        int  refresh_count = 0;
        bool auto_complete = true; // if true, on_refresh calls done() immediately

        void SetUp() override
        {
            widget->controller       = controller;
            widget->child            = std::make_shared<cw::SizedBox>(400.0f, 2000.0f);
            widget->trigger_distance = 80.0f;
            widget->on_refresh = [this](std::function<void()> done)
            {
                ++refresh_count;
                if (auto_complete) done();
            };

            root_element = widget->createElement();
            root_element->mount(nullptr);
        }
    };
} // namespace

TEST_F(RefreshIndicatorFixture, FiresOnRefreshExactlyOnceWhenReleasedPastThreshold)
{
    controller->notifyOverscroll(40.0f);  // dragging, below threshold
    EXPECT_EQ(refresh_count, 0);

    controller->notifyOverscroll(90.0f);  // armed — past the 80px threshold
    EXPECT_EQ(refresh_count, 0);          // not yet — still needs a release

    controller->notifyOverscroll(60.0f);  // first decrease after being armed == release
    EXPECT_EQ(refresh_count, 1);
}

TEST_F(RefreshIndicatorFixture, PullingFurtherPastThresholdWithoutReleasingDoesNotTriggerYet)
{
    controller->notifyOverscroll(90.0f);  // armed
    controller->notifyOverscroll(120.0f); // still increasing — still an active drag, not a release
    EXPECT_EQ(refresh_count, 0);

    controller->notifyOverscroll(150.0f); // increasing again
    EXPECT_EQ(refresh_count, 0);

    controller->notifyOverscroll(100.0f); // now decreases — this is the release
    EXPECT_EQ(refresh_count, 1);
}

TEST_F(RefreshIndicatorFixture, ReleasingBeforeThresholdDoesNotTrigger)
{
    controller->notifyOverscroll(40.0f);
    controller->notifyOverscroll(20.0f); // decreased, but was never armed (never reached 80px)
    controller->notifyOverscroll(0.0f);
    EXPECT_EQ(refresh_count, 0);
}

TEST_F(RefreshIndicatorFixture, OnRefreshDoneCallbackAllowsANewCycleToTriggerAgain)
{
    controller->notifyOverscroll(90.0f);
    controller->notifyOverscroll(50.0f); // release #1 -> triggers, auto_complete calls done() synchronously
    EXPECT_EQ(refresh_count, 1);

    // A second full pull cycle after settling back to idle must be able to
    // trigger again.
    controller->notifyOverscroll(90.0f);
    controller->notifyOverscroll(50.0f); // release #2
    EXPECT_EQ(refresh_count, 2);
}

TEST_F(RefreshIndicatorFixture, DoesNotReTriggerWhileARefreshIsStillInFlight)
{
    auto_complete = false; // on_refresh does NOT call done() — simulates an in-flight async op

    controller->notifyOverscroll(90.0f);
    controller->notifyOverscroll(50.0f); // release -> triggers, refreshing_ stays true (no done() yet)
    EXPECT_EQ(refresh_count, 1);

    // A further pull-and-release while still refreshing must be ignored.
    controller->notifyOverscroll(90.0f);
    controller->notifyOverscroll(50.0f);
    EXPECT_EQ(refresh_count, 1);
}

// ---------------------------------------------------------------------------
// Real-pipeline regression test — user-reported bug: "start the movement
// down but don't finish the movement and cancel it, the refresh indicator
// does not disappear."
//
// Unlike RefreshIndicatorFixture above (which drives RefreshIndicatorState
// via synthetic notifyOverscroll() calls with hand-picked values, bypassing
// the real render object entirely) and ScrollViewOverscrollFixture (which
// drives a real render object with real PointerEvents but never mounts a
// RefreshIndicator to observe), this fixture combines both: a real
// RenderSingleChildScrollView attached to a real PointerDispatcher, AND a
// real, separately-mounted RefreshIndicator sharing the same
// ScrollController — so the RefreshIndicator observes the actual sequence
// of notifyOverscroll() values a genuine partial-drag-then-release produces
// via the render object's own onTick() spring-back, not values chosen to
// match what the state machine expects.
// ---------------------------------------------------------------------------

namespace
{
    struct RealPipelineRefreshFixture : public ::testing::Test
    {
        cw::RenderSingleChildScrollView sv;
        std::shared_ptr<cw::ScrollController> controller = std::make_shared<cw::ScrollController>();
        std::shared_ptr<cw::PointerDispatcher> dispatcher;
        std::shared_ptr<cw::RefreshIndicator>  widget = std::make_shared<cw::RefreshIndicator>();
        std::shared_ptr<cw::Element>           root_element;

        void SetUp() override
        {
            sv.scroll_axis = cw::Axis::vertical;
            sv.setController(controller);

            auto child = std::make_shared<cw::RenderSizedBox>();
            child->width  = 400.0f;
            child->height = 2000.0f;
            sv.setChild(child);

            auto root = std::shared_ptr<cw::RenderBox>(&sv, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            sv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
            sv.attach();

            // Mounted separately from `sv` above -- what matters is that it
            // shares the same ScrollController, so it observes the real
            // notifyOverscroll() sequence `sv`'s pointer/tick handling
            // produces. Its own `child` field is inert (never hit-tested in
            // this test); only whether *its* built tree still contains the
            // reveal overlay is under test.
            widget->controller = controller;
            widget->child      = std::make_shared<cw::SizedBox>(400.0f, 2000.0f);
            widget->trigger_distance = 80.0f;
            root_element = widget->createElement();
            root_element->mount(nullptr);
        }

        void TearDown() override
        {
            sv.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }

        // RefreshIndicator::build() returns a Stack directly, so the nearest
        // RenderObjectElement below the StatefulElement owns the RenderStack.
        // 1 child = just `child` (indicator hidden); 2 = indicator overlay
        // also present.
        size_t indicatorStackChildCount() const
        {
            auto* ro_element = root_element->findDescendantRenderObjectElement();
            EXPECT_NE(ro_element, nullptr);
            auto* render_stack = dynamic_cast<cw::RenderStack*>(ro_element->renderObject());
            EXPECT_NE(render_stack, nullptr);
            return render_stack->childCount();
        }
    };
} // namespace

TEST_F(RealPipelineRefreshFixture, IndicatorHidesAgainAfterRealPartialDragThenRelease)
{
    ASSERT_EQ(indicatorStackChildCount(), 1u) << "hidden before any drag";

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // 50px pull -- well below the 80px trigger
    dispatcher->handlePointerEvent(move);

    ASSERT_EQ(indicatorStackChildCount(), 2u) << "revealed mid-drag";

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);

    // Drive real ticks -- same count/cadence ScrollViewOverscrollFixture's
    // ReleaseSpringBackEventuallySettlesOverscrollToZero uses to let the
    // render object's onTick() spring-back fully settle raw_offset_ back
    // to the boundary.
    for (int i = 0; i < 40; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        dispatcher->tick(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()));
    }

    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);
    EXPECT_EQ(indicatorStackChildCount(), 1u)
        << "must hide again once the spring-back has fully settled";
}

// Same scenario via a real PointerEventKind::cancel instead of `up` -- the
// literal word the user used ("cancel it"), distinct from a normal release
// (e.g. macOS sends `cancel` rather than `up` when the drag exits the
// window). RenderSingleChildScrollView::onPointerEvent's cancel case clears
// panning_ the same way up does, so the spring-back should behave
// identically -- confirms that's actually true rather than assuming it.
TEST_F(RealPipelineRefreshFixture, IndicatorHidesAgainAfterRealPartialDragThenCancel)
{
    ASSERT_EQ(indicatorStackChildCount(), 1u);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 50.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // 50px pull -- below the 80px trigger
    dispatcher->handlePointerEvent(move);

    ASSERT_EQ(indicatorStackChildCount(), 2u);

    cw::PointerEvent cancel;
    cancel.kind = cw::PointerEventKind::cancel;
    dispatcher->handlePointerEvent(cancel);

    for (int i = 0; i < 40; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        dispatcher->tick(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()));
    }

    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);
    EXPECT_EQ(indicatorStackChildCount(), 1u)
        << "must hide again after a cancel event too, not just a normal up";
}
