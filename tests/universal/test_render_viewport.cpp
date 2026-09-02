#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/render_sliver_persistent_header.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Fixed-extent sliver test double, mirroring test_render_sliver.cpp's own
    // ViewportTestSliver (Stage 1) exactly: scroll_extent is constant, paint/
    // layout/hit_test extent are derived deterministically from
    // sliver_constraints_ the same way a real fixed-extent list sliver (a
    // later stage) would.
    class ViewportTestSliver : public cw::RenderSliver
    {
    public:
        explicit ViewportTestSliver(float scroll_extent) : scroll_extent_(scroll_extent) {}

        int performLayoutSliverCallCount() const noexcept { return call_count_; }

    protected:
        void performLayoutSliver() override
        {
            ++call_count_;
            const float paint = std::clamp(
                scroll_extent_ - sliver_constraints_.scroll_offset,
                0.0f, sliver_constraints_.remaining_paint_extent);

            geometry_ = cw::SliverGeometry{};
            geometry_.scroll_extent    = scroll_extent_;
            geometry_.paint_extent     = paint;
            geometry_.layout_extent    = paint;
            geometry_.max_paint_extent = scroll_extent_;
            geometry_.hit_test_extent  = paint;
            geometry_.visible          = paint > 0.0f;
        }

        void performPaint(cw::PaintContext&, const cw::Offset&) override {}

    private:
        float scroll_extent_;
        int   call_count_ = 0;
    };

    // Returns a non-null scroll_offset_correction on its first
    // performLayoutSliver() call only; every subsequent call produces stable,
    // real geometry from the (by-then corrected) constraints. Used to verify
    // RenderViewport::performLayout()'s correction-reflow loop.
    class CorrectingSliver : public cw::RenderSliver
    {
    public:
        CorrectingSliver(float scroll_extent, float correction)
            : scroll_extent_(scroll_extent), correction_(correction) {}

        int performLayoutSliverCallCount() const noexcept { return call_count_; }

    protected:
        void performLayoutSliver() override
        {
            ++call_count_;
            geometry_ = cw::SliverGeometry{};

            if (!already_corrected_)
            {
                already_corrected_ = true;
                geometry_.scroll_offset_correction = correction_;
                return;
            }

            const float paint = std::clamp(
                scroll_extent_ - sliver_constraints_.scroll_offset,
                0.0f, sliver_constraints_.remaining_paint_extent);
            geometry_.scroll_extent    = scroll_extent_;
            geometry_.paint_extent     = paint;
            geometry_.layout_extent    = paint;
            geometry_.max_paint_extent = scroll_extent_;
            geometry_.hit_test_extent  = paint;
            geometry_.visible          = paint > 0.0f;
        }

        void performPaint(cw::PaintContext&, const cw::Offset&) override {}

    private:
        float scroll_extent_;
        float correction_;
        bool  already_corrected_ = false;
        int   call_count_        = 0;
    };

    static void doLayout(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Basic coordination — running-totals threading between siblings
// ---------------------------------------------------------------------------

TEST(RenderViewport, ConstraintsThreadingAtScrollStart)
{
    cw::RenderViewport vp;
    auto s0 = std::make_shared<ViewportTestSliver>(200.0f);
    auto s1 = std::make_shared<ViewportTestSliver>(300.0f);
    vp.insertChild(s0, 0);
    vp.insertChild(s1, 1);

    doLayout(vp, 400.0f, 250.0f);

    const auto& c0 = s0->sliverConstraints();
    EXPECT_FLOAT_EQ(c0.scroll_offset, 0.0f);
    EXPECT_FLOAT_EQ(c0.preceding_scroll_extent, 0.0f);
    EXPECT_FLOAT_EQ(c0.remaining_paint_extent, 250.0f);
    EXPECT_FLOAT_EQ(c0.viewport_main_axis_extent, 250.0f);
    EXPECT_FLOAT_EQ(c0.cross_axis_extent, 400.0f);

    const auto& c1 = s1->sliverConstraints();
    EXPECT_FLOAT_EQ(c1.scroll_offset, 0.0f); // hasn't been scrolled into yet
    EXPECT_FLOAT_EQ(c1.preceding_scroll_extent, 200.0f); // s0's full scroll_extent
    EXPECT_FLOAT_EQ(c1.remaining_paint_extent, 50.0f); // 250 - s0's 200px paint
}

TEST(RenderViewport, ConstraintsThreadingAfterScrolling)
{
    cw::RenderViewport vp;
    auto s0 = std::make_shared<ViewportTestSliver>(200.0f);
    auto s1 = std::make_shared<ViewportTestSliver>(300.0f);
    vp.insertChild(s0, 0);
    vp.insertChild(s1, 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 250.0f); // establishes extents
    controller->jumpTo(100.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 250.0f); // re-run at scroll=100

    const auto& c0 = s0->sliverConstraints();
    EXPECT_FLOAT_EQ(c0.scroll_offset, 100.0f);
    EXPECT_FLOAT_EQ(c0.preceding_scroll_extent, 0.0f);
    EXPECT_FLOAT_EQ(c0.remaining_paint_extent, 250.0f);

    const auto& c1 = s1->sliverConstraints();
    // s0 painted 100px of its remaining 100px content (200 scroll_extent -
    // 100 scroll_offset), fully consumed by scroll -- s1 hasn't been reached
    // by the visible edge yet, so its own scroll_offset is still 0.
    EXPECT_FLOAT_EQ(c1.scroll_offset, 0.0f);
    EXPECT_FLOAT_EQ(c1.preceding_scroll_extent, 200.0f);
    EXPECT_FLOAT_EQ(c1.remaining_paint_extent, 250.0f);
}

// ---------------------------------------------------------------------------
// 2. Aggregate extent
// ---------------------------------------------------------------------------

TEST(RenderViewport, MaxScrollExtentIsSumOfChildrenMinusViewport)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(200.0f), 0);
    vp.insertChild(std::make_shared<ViewportTestSliver>(300.0f), 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 250.0f);

    EXPECT_FLOAT_EQ(controller->minScrollExtent(), 0.0f);
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 250.0f); // 500 - 250
}

TEST(RenderViewport, MaxScrollExtentClampsToZeroWhenContentFitsViewport)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(50.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 250.0f);

    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 0.0f);
}

// ---------------------------------------------------------------------------
// 3. Correction-reflow loop — the piece flagged as not deferrable
// ---------------------------------------------------------------------------

TEST(RenderViewport, ScrollOffsetCorrectionReRunsWholePassWithAdjustedOffset)
{
    cw::RenderViewport vp;
    auto correcting = std::make_shared<CorrectingSliver>(30.0f, 100.0f);
    auto tracker     = std::make_shared<ViewportTestSliver>(200.0f);
    vp.insertChild(correcting, 0);
    vp.insertChild(tracker, 1);

    doLayout(vp, 400.0f, 300.0f);

    // First call reported the correction and bailed without real geometry;
    // second call (after the whole pass re-ran with the adjusted offset)
    // produced the real, stable geometry.
    EXPECT_EQ(correcting->performLayoutSliverCallCount(), 2);

    // tracker is only ever laid out once -- the FIRST cycle's inner loop
    // breaks out before reaching it at all, so it must see constraints from
    // the corrected cycle, not some stale pre-correction snapshot.
    EXPECT_EQ(tracker->performLayoutSliverCallCount(), 1);

    const auto& tc = tracker->sliverConstraints();
    EXPECT_FLOAT_EQ(tc.preceding_scroll_extent, 30.0f); // correcting sliver's own scroll_extent, unaffected by the correction
    // Without the correction, scroll(0) - preceding(30) would clamp to 0,
    // same as with it -- so this alone wouldn't prove anything. The
    // discriminating assertion is the correcting sliver's OWN post-
    // correction constraints below, which only the reflow could produce.
    EXPECT_FLOAT_EQ(tc.scroll_offset, 70.0f); // max(0, 0 + 100 correction - 30 preceding)

    const auto& cc = correcting->sliverConstraints();
    EXPECT_FLOAT_EQ(cc.scroll_offset, 100.0f); // max(0, 0 + 100 correction - 0 preceding)
}

TEST(RenderViewport, RunawayCorrectionIsBoundedAndDoesNotHang)
{
    // A sliver that corrects every single call would spin forever without a
    // cycle cap -- assert layout still terminates and the sliver was capped
    // at a bounded number of attempts (kMaxLayoutCycles + the final,
    // accepted-as-is pass).
    class AlwaysCorrectingSliver : public cw::RenderSliver
    {
    public:
        int callCount() const noexcept { return call_count_; }

    protected:
        void performLayoutSliver() override
        {
            ++call_count_;
            geometry_ = cw::SliverGeometry{};
            geometry_.scroll_offset_correction = 1.0f;
        }
        void performPaint(cw::PaintContext&, const cw::Offset&) override {}

    private:
        int call_count_ = 0;
    };

    cw::RenderViewport vp;
    auto s = std::make_shared<AlwaysCorrectingSliver>();
    vp.insertChild(s, 0);

    doLayout(vp, 400.0f, 300.0f); // must return, not hang

    EXPECT_GE(s->callCount(), 10);
    EXPECT_LT(s->callCount(), 100);
}

// ---------------------------------------------------------------------------
// 4. Paint offsets
// ---------------------------------------------------------------------------

TEST(RenderViewport, LayoutOffsetsReflectScrollPosition)
{
    cw::RenderViewport vp;
    auto a = std::make_shared<ViewportTestSliver>(100.0f); // will be fully scrolled past
    auto b = std::make_shared<ViewportTestSliver>(100.0f); // partially visible
    auto c = std::make_shared<ViewportTestSliver>(100.0f);
    auto d = std::make_shared<ViewportTestSliver>(50.0f);  // fully below, budget exhausted
    vp.insertChild(a, 0);
    vp.insertChild(b, 1);
    vp.insertChild(c, 2);
    vp.insertChild(d, 3);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 150.0f); // establishes extents (total 350, viewport 150 -> max 200)
    controller->jumpTo(120.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -120.0f);
    EXPECT_FALSE(a->geometry().visible);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), -20.0f);
    EXPECT_TRUE(b->geometry().visible);

    EXPECT_GT(vp.layoutOffsetAt(3), 150.0f); // starts past the viewport's own extent
    EXPECT_FALSE(d->geometry().visible);
}

// ---------------------------------------------------------------------------
// Child management
// ---------------------------------------------------------------------------

TEST(RenderViewport, InsertClearTruncateChildren)
{
    cw::RenderViewport vp;
    auto s0 = std::make_shared<ViewportTestSliver>(100.0f);
    auto s1 = std::make_shared<ViewportTestSliver>(100.0f);
    vp.insertChild(s0, 0);
    vp.insertChild(s1, 1);
    EXPECT_EQ(vp.sliverCount(), 2u);
    EXPECT_EQ(vp.sliverAt(0), s0.get());
    EXPECT_EQ(vp.sliverAt(1), s1.get());

    vp.truncateChildren(1);
    EXPECT_EQ(vp.sliverCount(), 1u);
    EXPECT_EQ(vp.sliverAt(0), s0.get());

    vp.clearChildren();
    EXPECT_EQ(vp.sliverCount(), 0u);
}

TEST(RenderViewport, VisitSliverChildrenVisitsEveryChild)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(100.0f), 0);
    vp.insertChild(std::make_shared<ViewportTestSliver>(100.0f), 1);

    int count = 0;
    vp.visitSliverChildren([&](cw::RenderSliver*) { ++count; });
    EXPECT_EQ(count, 2);
}

// ---------------------------------------------------------------------------
// 5. Drag / momentum mechanics — same test shape as RenderListView's own
// ---------------------------------------------------------------------------

TEST(RenderViewport, PanGestureScrollsController)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // crosses pan slop, doesn't scroll yet
    dispatcher.handlePointerEvent(move);

    move.position = {0.0f, 50.0f}; // dy=-50 applied as scroll
    dispatcher.handlePointerEvent(move);

    EXPECT_GT(controller->offset(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderViewport, WheelEventScrollsController)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    cw::PointerEvent scroll;
    scroll.kind           = cw::PointerEventKind::scroll;
    scroll.position       = {0.0f, 0.0f};
    scroll.scroll_delta_y = 100.0f;
    dispatcher.handlePointerEvent(scroll);

    EXPECT_FLOAT_EQ(controller->offset(), 100.0f);

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderViewport, MomentumContinuesAfterRelease)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(5000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    auto nowMs = [] {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 190.0f};
    dispatcher.handlePointerEvent(down);

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

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    const float offset_at_release = controller->offset();

    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());

    EXPECT_GT(controller->offset(), offset_at_release)
        << "viewport should keep scrolling under momentum after release, not stop immediately";

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// ---------------------------------------------------------------------------
// 6. Pinning (Stage 5) — RenderSliverPersistentHeader integration. New
// territory with no prior precedent; every expected number below was worked
// through by hand against RenderViewport::performLayout()'s actual formulas
// before being asserted (see the Stage 5 plan's own worked example, which
// this test's first case reproduces with header height 80).
// ---------------------------------------------------------------------------

TEST(RenderViewport, SinglePinnedHeaderStaysAtTopWhileBodyScrollsAndClipsUnderIt)
{
    cw::RenderViewport vp;
    auto header = std::make_shared<cw::RenderSliverPersistentHeader>();
    header->min_extent = 80.0f;
    header->max_extent = 80.0f; // fixed, non-collapsing
    auto body = std::make_shared<ViewportTestSliver>(300.0f);
    vp.insertChild(header, 0);
    vp.insertChild(body, 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 150.0f); // total content 380, viewport 150 -> max scroll 230

    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 230.0f);

    // At scroll 0: header sits at the top, body is flush against its bottom
    // edge (80px), matching the plan's worked example exactly.
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 80.0f);
    EXPECT_TRUE(vp.isPinnedObstructionAt(0));
    EXPECT_FALSE(vp.isPinnedObstructionAt(1));
    EXPECT_FLOAT_EQ(vp.clipFloorAt(1), 80.0f); // == header's height, even though not yet "needed"

    // Scroll deep (200 of the available 230) -- header stays pinned at 0
    // (never scrolls away), body's own natural, unclamped position keeps
    // advancing upward past the header's floor, which is exactly why
    // clipFloorAt(1) exists: without a clip, body would paint straight
    // through the header's reserved 80px region.
    controller->jumpTo(200.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f); // still pinned
    EXPECT_FLOAT_EQ(header->geometry().paint_extent, 80.0f); // never shrinks (min == max)
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), -120.0f); // 80 - 200, unclamped
    EXPECT_FLOAT_EQ(vp.clipFloorAt(1), 80.0f);
    EXPECT_TRUE(vp.isPinnedObstructionAt(0));
    EXPECT_FALSE(vp.isPinnedObstructionAt(1));
}

TEST(RenderViewport, TwoStackedPinnedHeadersClampToEachOtherAndSumIntoBodyClipFloor)
{
    cw::RenderViewport vp;
    auto header1 = std::make_shared<cw::RenderSliverPersistentHeader>();
    header1->min_extent = 60.0f;
    header1->max_extent = 60.0f;
    auto header2 = std::make_shared<cw::RenderSliverPersistentHeader>();
    header2->min_extent = 50.0f;
    header2->max_extent = 50.0f;
    auto body = std::make_shared<ViewportTestSliver>(300.0f);
    vp.insertChild(header1, 0);
    vp.insertChild(header2, 1);
    vp.insertChild(body, 2);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 150.0f); // total content 410, viewport 150 -> max scroll 260

    // At scroll 0: headers stack directly on top of each other, body flush
    // against the second header's bottom edge.
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 60.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(2), 110.0f);
    EXPECT_FLOAT_EQ(vp.clipFloorAt(2), 110.0f); // 60 + 50

    // Scroll deep (200 of 260) -- both headers pin, second stacks below the
    // first (clamped to header1's height, NOT its own unclamped natural
    // position), body's clip floor reflects the sum of both.
    controller->jumpTo(200.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);   // header1 pinned at top
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 60.0f);  // header2 stacks right below header1
    EXPECT_TRUE(vp.isPinnedObstructionAt(0));
    EXPECT_TRUE(vp.isPinnedObstructionAt(1));
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(2), -90.0f); // body's unclamped natural position
    EXPECT_FLOAT_EQ(vp.clipFloorAt(2), 110.0f);    // 60 + 50, unchanged
}

TEST(RenderViewport, CollapsingHeaderTracksBodyDuringCollapseThenClipsOncePinned)
{
    cw::RenderViewport vp;
    auto header = std::make_shared<cw::RenderSliverPersistentHeader>();
    header->min_extent = 30.0f;
    header->max_extent = 100.0f; // 70px collapse range
    auto body = std::make_shared<ViewportTestSliver>(300.0f);
    vp.insertChild(header, 0);
    vp.insertChild(body, 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 150.0f); // total content 400, viewport 150 -> max scroll 250

    // Mid-collapse (scroll 40 of the 70px collapse range): header stays
    // pinned at position 0 (only its SIZE shrinks, not its position), body
    // stays exactly flush against the header's current (shrunk) bottom edge
    // -- no gap, no overlap, confirmed algebraically in the plan.
    controller->jumpTo(40.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(header->geometry().paint_extent, 60.0f); // 100 - 40
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 60.0f); // flush against header's current bottom edge
    EXPECT_FLOAT_EQ(vp.clipFloorAt(1), 30.0f); // == min_extent, harmless-but-inert during collapse

    // Fully collapsed and pinned (scroll 150, well past the 70px collapse
    // range): header holds at min_extent, body's unclamped position has now
    // moved past the header's resting floor -- this is exactly the case the
    // clip exists for.
    controller->jumpTo(150.0f);
    vp.markNeedsLayout();
    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f); // still pinned
    EXPECT_FLOAT_EQ(header->geometry().paint_extent, 30.0f); // holds at min_extent, doesn't keep shrinking
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), -50.0f); // 100 - 150, unclamped -- now past the header's floor (30)
    EXPECT_FLOAT_EQ(vp.clipFloorAt(1), 30.0f);
}

TEST(RenderViewport, NoPinnedHeadersLeavesClipFloorAndObstructionFlagsAtDefault)
{
    // Regression guard for the gated-no-op claim: with only ordinary
    // (non-obstructing) slivers, every new Stage 5 field must sit at its
    // inert default and every layout_offset must match the pre-Stage-5
    // formula exactly -- re-derives the same numbers
    // LayoutOffsetsReflectScrollPosition already asserts, through the new
    // accessors this time.
    cw::RenderViewport vp;
    auto a = std::make_shared<ViewportTestSliver>(100.0f);
    auto b = std::make_shared<ViewportTestSliver>(100.0f);
    vp.insertChild(a, 0);
    vp.insertChild(b, 1);

    doLayout(vp, 400.0f, 150.0f);

    EXPECT_FALSE(vp.isPinnedObstructionAt(0));
    EXPECT_FALSE(vp.isPinnedObstructionAt(1));
    EXPECT_FLOAT_EQ(vp.clipFloorAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.clipFloorAt(1), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 100.0f);
}

// ---------------------------------------------------------------------------
// 7. applyExternalScrollDelta() — the NestedScrollView Stage 3 primitive. A
// thin wrapper around the same private applyScrollDelta() the pointer path
// already uses (RenderViewport's own calls markNeedsLayout(), not
// markNeedsPaint(), unlike RenderListView/RenderSingleChildScrollView --
// deliberate and unchanged by this wrapper), so these confirm the return
// value is genuinely physics-derived and that both the controller-attached
// and internal_offset_ paths are reachable through it.
// ---------------------------------------------------------------------------

TEST(RenderViewport, ApplyExternalScrollDeltaInBoundsWithController)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 200.0f); // content 1000, viewport 200 -> max_extent_ 800

    const float applied = vp.applyExternalScrollDelta(150.0f);
    EXPECT_FLOAT_EQ(applied, 150.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 150.0f);
}

TEST(RenderViewport, ApplyExternalScrollDeltaClampsAtBoundaryUnderClampingPhysics)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doLayout(vp, 400.0f, 200.0f); // max_extent_ 800

    const float applied = vp.applyExternalScrollDelta(2000.0f); // well past the 800 boundary
    EXPECT_FLOAT_EQ(applied, 800.0f); // only the portion up to the boundary
    EXPECT_FLOAT_EQ(controller->offset(), 800.0f);
}

TEST(RenderViewport, ApplyExternalScrollDeltaRubberBandsUnderBouncingPhysics)
{
    // Deliberately no ScrollController here (unlike the two cases above) --
    // ScrollController::setOffset() (scroll_controller.cpp) hard-clamps to
    // [min,max] on every jumpTo(), independent of physics, so a
    // controller-attached scrollOffset() can never surface rubber-banded
    // overscroll (that's what the separate raw-offset/notifyOverscroll()
    // channel is for). The internal_offset_ path below stores whatever
    // physics computes directly, with no further clamping, so it's the one
    // that actually demonstrates this method delegating to physics.
    cw::RenderViewport vp;
    vp.physics = std::make_shared<cw::BouncingScrollPhysics>();
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    doLayout(vp, 400.0f, 200.0f); // max_extent_ 800

    const float applied = vp.applyExternalScrollDelta(1000.0f); // 200px past the boundary
    // Resistance-damped past the boundary -- neither the hard 800 clamp
    // from the ClampingScrollPhysics case above, nor the full, undamped 1000.
    EXPECT_GT(applied, 800.0f);
    EXPECT_LT(applied, 1000.0f);
}

TEST(RenderViewport, ApplyExternalScrollDeltaWorksWithoutController)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    doLayout(vp, 400.0f, 200.0f); // no controller -> internal_offset_ path

    const float applied = vp.applyExternalScrollDelta(100.0f);
    EXPECT_FLOAT_EQ(applied, 100.0f);
}

// ---------------------------------------------------------------------------
// external_delta_redirect -- the NestedScrollView coordinator hook. Unset
// (default) must reproduce the exact same numbers PanGestureScrollsController
// exercises with a real drag -- the sharpest possible regression check.
// ---------------------------------------------------------------------------

TEST(RenderViewport, PanGestureWithRedirectHookUnsetMatchesOriginalBehavior)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();
    // vp.external_delta_redirect left unset (nullptr) deliberately.

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // dy=-50, crosses slop, applies immediately
    dispatcher.handlePointerEvent(move);

    move.position = {0.0f, 50.0f}; // dy=-50 more
    dispatcher.handlePointerEvent(move);

    EXPECT_FLOAT_EQ(controller->offset(), 100.0f); // 50 + 50, worked by hand

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderViewport, PanGestureRoutesThroughRedirectHookWhenSet)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    std::vector<float> redirected_deltas;
    vp.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 100.0f}; // dy=-50
    dispatcher.handlePointerEvent(move);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 50.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f); // redirect never applied anything itself

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderViewport, WheelEventRoutesThroughRedirectHookWhenSet)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(1000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    std::vector<float> redirected_deltas;
    vp.external_delta_redirect = [&](float d) { redirected_deltas.push_back(d); };

    cw::PointerEvent scroll;
    scroll.kind           = cw::PointerEventKind::scroll;
    scroll.position       = {0.0f, 0.0f};
    scroll.scroll_delta_y = 75.0f;
    dispatcher.handlePointerEvent(scroll);

    ASSERT_EQ(redirected_deltas.size(), 1u);
    EXPECT_FLOAT_EQ(redirected_deltas[0], 75.0f);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// onTick()'s own spring-back/momentum must NOT be redirected -- see
// RenderListView's own MomentumIsNotRedirectedEvenWithHookSet for the same
// reasoning.
TEST(RenderViewport, MomentumIsNotRedirectedEvenWithHookSet)
{
    cw::RenderViewport vp;
    vp.insertChild(std::make_shared<ViewportTestSliver>(5000.0f), 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    auto root = std::shared_ptr<cw::RenderBox>(&vp, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    doLayout(vp, 400.0f, 200.0f);
    vp.attach();

    int redirect_call_count = 0;
    vp.external_delta_redirect = [&](float) { ++redirect_call_count; };

    auto nowMs = [] {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 190.0f};
    dispatcher.handlePointerEvent(down);

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

    const int calls_during_drag = redirect_call_count;
    EXPECT_GT(calls_during_drag, 0);
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f); // redirect never applied anything to vp itself

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    dispatcher.tick(nowMs());

    EXPECT_GT(controller->offset(), 0.0f)
        << "momentum should still move the viewport via onTick()'s own direct applyScrollDelta()";
    EXPECT_EQ(redirect_call_count, calls_during_drag)
        << "onTick()'s spring/momentum calls must not go through external_delta_redirect";

    vp.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
