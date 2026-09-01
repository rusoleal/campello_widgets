#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver_persistent_header.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Reuses the production RenderSizedBox as the fixed-size RenderBox under
    // test, mirroring Stage 3/4's own fixtures. Named distinctly from the
    // other sliver test files' own helpers -- Unity Build merges all test
    // .cpp files into one TU, so a same-named free function/static helper
    // across files is a redefinition error (bit both Stage 3 and Stage 4).
    std::shared_ptr<cw::RenderSizedBox> makeFixedHeaderChildBox()
    {
        return std::make_shared<cw::RenderSizedBox>();
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Standalone geometry correctness
// ---------------------------------------------------------------------------

TEST(RenderSliverPersistentHeader, FullyExpandedAtScrollStart)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 40.0f;
    header.max_extent = 120.0f;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset          = 0.0f;
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    const auto& g = header.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 120.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 120.0f);
    EXPECT_FLOAT_EQ(g.paint_origin, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 120.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 120.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 40.0f); // == min_extent, even at scroll == 0
    EXPECT_FLOAT_EQ(g.hit_test_extent, 120.0f);
    EXPECT_FLOAT_EQ(g.cache_extent, 120.0f);
    EXPECT_FALSE(g.scroll_offset_correction.has_value());
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);
}

TEST(RenderSliverPersistentHeader, MidCollapseInterpolatesLinearly)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 40.0f;
    header.max_extent = 120.0f; // collapse range is 80px

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset          = 30.0f; // 30 of the 80px collapse range consumed
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    const auto& g = header.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 120.0f); // constant -- always the full natural size
    EXPECT_FLOAT_EQ(g.paint_extent, 90.0f);   // 120 - 30
    EXPECT_FLOAT_EQ(g.layout_extent, 90.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 40.0f); // constant regardless of collapse phase
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // shrink > 0
}

TEST(RenderSliverPersistentHeader, FullyCollapsedHoldsAtMinExtent)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 40.0f;
    header.max_extent = 120.0f;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset          = 200.0f; // well past the 80px collapse range
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    const auto& g = header.geometry();
    EXPECT_FLOAT_EQ(g.paint_extent, 40.0f); // == min_extent, not negative/shrinking further
    EXPECT_FLOAT_EQ(g.layout_extent, 40.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 40.0f);
    EXPECT_TRUE(g.visible);

    // Scrolling even further must not shrink it any further -- this is the
    // "holds at the floor forever after" behavior the pin depends on.
    c.scroll_offset = 5000.0f;
    header.markNeedsLayout();
    header.layoutSliver(c);
    EXPECT_FLOAT_EQ(header.geometry().paint_extent, 40.0f);
}

TEST(RenderSliverPersistentHeader, MinEqualsMaxIsAFixedNonCollapsingHeader)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 80.0f;
    header.max_extent = 80.0f; // no collapse range at all

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;

    c.scroll_offset = 0.0f;
    header.layoutSliver(c);
    EXPECT_FLOAT_EQ(header.geometry().paint_extent, 80.0f);
    EXPECT_FLOAT_EQ(header.geometry().max_scroll_obstruction_extent, 80.0f);

    c.scroll_offset = 500.0f; // any scroll -- always shows full height
    header.markNeedsLayout();
    header.layoutSliver(c);
    EXPECT_FLOAT_EQ(header.geometry().paint_extent, 80.0f);
    EXPECT_FLOAT_EQ(header.geometry().max_scroll_obstruction_extent, 80.0f);
}

TEST(RenderSliverPersistentHeader, PaintExtentClampedByRemainingBudget)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 0.0f;
    header.max_extent = 100.0f;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset          = 0.0f;
    c.remaining_paint_extent = 30.0f; // budget runs out before the full 100px
    header.layoutSliver(c);

    const auto& g = header.geometry();
    EXPECT_FLOAT_EQ(g.paint_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 100.0f); // unclamped natural size
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // paint_extent < current_extent
}

// ---------------------------------------------------------------------------
// 2. setChild() / child layout
// ---------------------------------------------------------------------------

TEST(RenderSliverPersistentHeader, SetChildReparentingMirrorsSiblingClasses)
{
    cw::RenderSliverPersistentHeader header;
    auto box_a = makeFixedHeaderChildBox();
    auto box_b = makeFixedHeaderChildBox();

    header.setChild(box_a);
    EXPECT_EQ(header.child(), box_a.get());
    EXPECT_EQ(box_a->parent(), &header);

    header.setChild(box_a); // same child again -- no-op, no reparent churn
    EXPECT_EQ(box_a->parent(), &header);

    header.setChild(box_b);
    EXPECT_EQ(box_a->parent(), nullptr);
    EXPECT_EQ(box_b->parent(), &header);
    EXPECT_EQ(header.child(), box_b.get());
}

TEST(RenderSliverPersistentHeader, ChildLaidOutTightToCurrentExtentBothAxes)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 40.0f;
    header.max_extent = 120.0f;
    auto box = makeFixedHeaderChildBox();
    header.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset          = 30.0f; // current_extent = 90
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    EXPECT_FLOAT_EQ(box->size().width, 300.0f);  // tight on cross axis
    EXPECT_FLOAT_EQ(box->size().height, 90.0f);  // tight on main axis (current_extent)
}

TEST(RenderSliverPersistentHeader, HorizontalAxisTightensWidthToCurrentExtent)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 20.0f;
    header.max_extent = 60.0f;
    auto box = makeFixedHeaderChildBox();
    header.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::horizontal;
    c.cross_axis_extent      = 200.0f;
    c.scroll_offset          = 0.0f;
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    EXPECT_FLOAT_EQ(box->size().width, 60.0f);   // tight on main axis (current_extent)
    EXPECT_FLOAT_EQ(box->size().height, 200.0f); // tight on cross axis
}

TEST(RenderSliverPersistentHeader, NoChildDoesNotCrashAndStillComputesGeometry)
{
    cw::RenderSliverPersistentHeader header;
    header.min_extent = 40.0f;
    header.max_extent = 120.0f;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    header.layoutSliver(c);

    // Unlike RenderSliverToBoxAdapter, this class's geometry is derived
    // purely from min_extent/max_extent, not the child -- so a missing
    // child produces the same real geometry, not all-zero.
    EXPECT_FLOAT_EQ(header.geometry().paint_extent, 120.0f);
    EXPECT_EQ(header.child(), nullptr);
}
