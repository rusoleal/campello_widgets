#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    struct ScrollViewFixture : public ::testing::Test
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

TEST_F(ScrollViewFixture, PanGestureScrollsWithoutCompetingRecognizer)
{
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // 100px up, exceeds 8px slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GT(controller->offset(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

// Regresses the same uncontested-arena-must-not-skip-the-slop-gate bug fixed
// for RenderTreeView/RenderListView/RenderGridView.
TEST_F(ScrollViewFixture, SubSlopJitterDoesNotScrollEvenWhenUncontested)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dy : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {0.0f, 150.0f + dy};
        dispatcher->handlePointerEvent(move);
    }

    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

// ---------------------------------------------------------------------------
// applyExternalScrollDelta() — the NestedScrollView Stage 3 primitive. A
// thin wrapper around the same private applyScrollDelta() the pointer path
// already uses, so these confirm the return value is genuinely
// physics-derived (not an echo of the input) and that both the
// controller-attached and internal_offset_ paths are reachable through it.
// ---------------------------------------------------------------------------

TEST_F(ScrollViewFixture, ApplyExternalScrollDeltaUpdatesAttachedController)
{
    // Fixture: 2000px child, 200px viewport -> max_extent_ 1800.
    const float applied = sv.applyExternalScrollDelta(150.0f);
    EXPECT_FLOAT_EQ(applied, 150.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 150.0f);
}

TEST_F(ScrollViewFixture, ApplyExternalScrollDeltaClampsAtBoundaryUnderClampingPhysics)
{
    const float applied = sv.applyExternalScrollDelta(5000.0f); // well past the 1800 boundary
    EXPECT_FLOAT_EQ(applied, 1800.0f); // only the portion up to the boundary
    EXPECT_FLOAT_EQ(controller->offset(), 1800.0f);
}

TEST(RenderSingleChildScrollView, ApplyExternalScrollDeltaInBoundsNoController)
{
    cw::RenderSingleChildScrollView sv;
    sv.scroll_axis = cw::Axis::vertical;
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 400.0f;
    child->height = 2000.0f;
    sv.setChild(child);
    sv.layout(cw::BoxConstraints::tight(400.0f, 200.0f)); // max_extent_ 1800

    const float applied = sv.applyExternalScrollDelta(100.0f);
    EXPECT_FLOAT_EQ(applied, 100.0f);
}

TEST(RenderSingleChildScrollView, ApplyExternalScrollDeltaRubberBandsUnderBouncingPhysics)
{
    cw::RenderSingleChildScrollView sv;
    sv.scroll_axis = cw::Axis::vertical;
    sv.setPhysics(std::make_shared<cw::BouncingScrollPhysics>());
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 400.0f;
    child->height = 2000.0f;
    sv.setChild(child);
    sv.layout(cw::BoxConstraints::tight(400.0f, 200.0f)); // max_extent_ 1800

    const float applied = sv.applyExternalScrollDelta(2000.0f); // 200px past the boundary
    // Resistance-damped past the boundary -- neither the hard 1800 clamp
    // from the ClampingScrollPhysics case above, nor the full, undamped 2000.
    EXPECT_GT(applied, 1800.0f);
    EXPECT_LT(applied, 2000.0f);
}

// ---------------------------------------------------------------------------
// external_delta_redirect -- the NestedScrollView coordinator hook.
// ---------------------------------------------------------------------------

TEST_F(ScrollViewFixture, PanGestureWithRedirectHookUnsetMatchesOriginalBehavior)
{
    // sv.external_delta_redirect left unset (nullptr) deliberately.
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // dy=-100, exceeds 36px touch slop, applies immediately
    dispatcher->handlePointerEvent(move);

    EXPECT_FLOAT_EQ(controller->offset(), 100.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

TEST_F(ScrollViewFixture, PanGestureRoutesThroughRedirectHookWhenSet)
{
    std::vector<float> redirected_deltas;
    sv.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // dy=-100
    dispatcher->handlePointerEvent(move);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 100.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f); // redirect never applied anything itself

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

TEST_F(ScrollViewFixture, WheelEventRoutesThroughRedirectHookWhenSet)
{
    std::vector<float> redirected_deltas;
    sv.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    cw::PointerEvent scroll;
    scroll.kind           = cw::PointerEventKind::scroll;
    scroll.position       = {0.0f, 0.0f};
    scroll.scroll_delta_y = 75.0f;
    dispatcher->handlePointerEvent(scroll);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 75.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);
}

// onTick()'s own spring-back/momentum must NOT be redirected -- see
// RenderListView's own MomentumIsNotRedirectedEvenWithHookSet for the same
// reasoning, adapted to this class's dispatcher.tick()-driven momentum path.
TEST_F(ScrollViewFixture, MomentumIsNotRedirectedEvenWithHookSet)
{
    int redirect_call_count = 0;
    sv.external_delta_redirect = [&](float) { ++redirect_call_count; };

    auto nowMs = [] {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    // Synthetic timestamp_ms values, +10ms per event, instead of real
    // std::this_thread::sleep_for() between dispatches -- VelocityTracker
    // computes elapsed time from PointerEvent::timestamp_ms, not real
    // wall-clock time, so this makes the drag's resulting release velocity
    // deterministic regardless of how long this process actually takes to
    // execute each line (see VelocityTracker's own doc comment).
    uint64_t t = 1'000;

    cw::PointerEvent down;
    down.kind         = cw::PointerEventKind::down;
    down.position     = {0.0f, 190.0f};
    down.timestamp_ms = t;
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    float y = 190.0f;
    for (int i = 1; i <= 5; i++)
    {
        t += 10;
        y -= 40.0f;
        move.position     = {0.0f, y};
        move.timestamp_ms = t;
        dispatcher->handlePointerEvent(move);
    }

    const int calls_during_drag = redirect_call_count;
    EXPECT_GT(calls_during_drag, 0);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f); // redirect never applied anything to sv itself

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);

    dispatcher->tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher->tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher->tick(nowMs());

    EXPECT_GT(controller->offset(), 0.0f)
        << "momentum should still move the view via onTick()'s own direct applyScrollDelta()";
    EXPECT_EQ(redirect_call_count, calls_during_drag)
        << "onTick()'s spring/momentum calls must not go through external_delta_redirect";
}
