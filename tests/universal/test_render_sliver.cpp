#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <algorithm>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Fixed-extent sliver test double, modeled on
    // test_render_object_paint_boundary_propagation.cpp's LeafRenderBox --
    // the simplest possible concrete RenderSliver. scroll_extent is a
    // constant set at construction; paint_extent/layout_extent/
    // hit_test_extent are derived from sliver_constraints_ the same way a
    // real fixed-extent list sliver (a later stage) would. Exposes a call
    // counter so tests can assert layoutSliver()'s early-return actually
    // skips a real re-layout when nothing changed.
    class TestRenderSliver : public cw::RenderSliver
    {
    public:
        explicit TestRenderSliver(float scroll_extent) : scroll_extent_(scroll_extent) {}

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
} // namespace

TEST(RenderSliver, LayoutSliverInvokesPerformLayoutSliverAndClearsNeedsLayout)
{
    TestRenderSliver sliver(200.0f);
    EXPECT_TRUE(sliver.needsLayout()) << "every RenderObject starts dirty by construction";

    cw::SliverConstraints c;
    c.remaining_paint_extent = 500.0f;
    sliver.layoutSliver(c);

    EXPECT_EQ(sliver.performLayoutSliverCallCount(), 1);
    EXPECT_FALSE(sliver.needsLayout());
}

TEST(RenderSliver, RepeatedIdenticalConstraintsWithNoDirtyFlagIsANoOp)
{
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.remaining_paint_extent = 500.0f;

    sliver.layoutSliver(c);
    ASSERT_EQ(sliver.performLayoutSliverCallCount(), 1);
    ASSERT_FALSE(sliver.needsLayout());

    // Same constraints again, and nothing marked it dirty in between --
    // mirrors RenderObject::layout()'s own early-return semantics exactly.
    sliver.layoutSliver(c);
    EXPECT_EQ(sliver.performLayoutSliverCallCount(), 1)
        << "layoutSliver() must not re-invoke performLayoutSliver() when "
           "neither the constraints changed nor needs_layout_ was set";
}

TEST(RenderSliver, ChangedConstraintsReLayoutsEvenWithoutAnExplicitMark)
{
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.remaining_paint_extent = 500.0f;

    sliver.layoutSliver(c);
    ASSERT_EQ(sliver.performLayoutSliverCallCount(), 1);

    c.scroll_offset = 50.0f; // a genuinely different SliverConstraints value
    sliver.layoutSliver(c);
    EXPECT_EQ(sliver.performLayoutSliverCallCount(), 2)
        << "a constraints change must trigger a real re-layout even when "
           "needs_layout_ was already false";
}

TEST(RenderSliver, ConstraintsChangeMarksNeedsPaintEvenWhenAlreadyClean)
{
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.remaining_paint_extent = 500.0f;

    sliver.layoutSliver(c);
    cw::PaintContext ctx(500.0f, 500.0f);
    sliver.paint(ctx, cw::Offset::zero());
    ASSERT_FALSE(sliver.needsPaint());

    c.scroll_offset = 50.0f;
    sliver.layoutSliver(c);
    EXPECT_TRUE(sliver.needsPaint())
        << "a constraints change must re-dirty paint, matching "
           "RenderObject::layout()'s must_repaint semantics";
}

TEST(RenderSliver, GeometryReflectsWhatPerformLayoutSliverComputed)
{
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.scroll_offset          = 50.0f;
    c.remaining_paint_extent = 500.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 200.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 150.0f); // 200 - 50 scroll_offset, well under the 500 budget
    EXPECT_FLOAT_EQ(g.max_paint_extent, 200.0f);
    EXPECT_TRUE(g.visible);
}

TEST(RenderSliver, InheritedPaintClearsNeedsPaintAfterARealCall)
{
    // Confirms the repurposed size_ write in layoutSliver() (see the
    // class's own doc comment) doesn't break the inherited
    // RenderObject::paint() path -- same assertion idiom as
    // test_render_object_paint_boundary_propagation.cpp's
    // StopsAtRepaintBoundaryAncestor test.
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    sliver.layoutSliver(c);

    ASSERT_TRUE(sliver.needsPaint()) << "every RenderObject starts dirty by construction";

    cw::PaintContext ctx(500.0f, 500.0f);
    sliver.paint(ctx, cw::Offset::zero());

    ASSERT_FALSE(sliver.needsPaint());
}

TEST(RenderSliver, SliverConstraintsAccessorReturnsWhatWasPassedIn)
{
    TestRenderSliver sliver(200.0f);
    cw::SliverConstraints c;
    c.scroll_offset      = 25.0f;
    c.cross_axis_extent  = 300.0f;
    sliver.layoutSliver(c);

    EXPECT_TRUE(sliver.sliverConstraints() == c);
}
