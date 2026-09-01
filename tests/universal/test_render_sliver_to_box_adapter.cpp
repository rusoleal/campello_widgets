#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver_to_box_adapter.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Reuses the production RenderSizedBox (no child, height set) as the
    // fixed-size RenderBox under test, mirroring the plan's guidance to
    // prefer real precedent over a synthetic fixture -- a childless
    // RenderSizedBox with only `height` set reports exactly (constraints'
    // tight cross-axis width, height) once laid out, with no bespoke test
    // double needed.
    std::shared_ptr<cw::RenderSizedBox> makeFixedHeightBox(float height)
    {
        auto box = std::make_shared<cw::RenderSizedBox>();
        box->height = height;
        return box;
    }

    static void doViewportLayout(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Standalone geometry correctness
// ---------------------------------------------------------------------------

TEST(RenderSliverToBoxAdapter, FullyVisible)
{
    cw::RenderSliverToBoxAdapter adapter;
    adapter.setChild(makeFixedHeightBox(100.0f));

    cw::SliverConstraints c;
    c.axis                      = cw::Axis::vertical;
    c.cross_axis_extent         = 300.0f;
    c.scroll_offset              = 0.0f;
    c.remaining_paint_extent    = 500.0f;
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_origin, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.cache_extent, 100.0f);
    EXPECT_FALSE(g.scroll_offset_correction.has_value());
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);

    ASSERT_NE(adapter.child(), nullptr);
    EXPECT_FLOAT_EQ(adapter.child()->size().width, 300.0f);
    EXPECT_FLOAT_EQ(adapter.child()->size().height, 100.0f);
}

TEST(RenderSliverToBoxAdapter, FullyScrolledPast)
{
    cw::RenderSliverToBoxAdapter adapter;
    adapter.setChild(makeFixedHeightBox(100.0f));

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 150.0f; // past the child's full 100px extent
    c.remaining_paint_extent = 500.0f;
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 100.0f);
    EXPECT_FALSE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // scroll_offset > 0
}

TEST(RenderSliverToBoxAdapter, PartiallyVisibleAtTop)
{
    cw::RenderSliverToBoxAdapter adapter;
    adapter.setChild(makeFixedHeightBox(100.0f));

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 40.0f; // top 40px already scrolled past
    c.remaining_paint_extent = 500.0f;
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 60.0f); // 100 - 40
    EXPECT_FLOAT_EQ(g.layout_extent, 60.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // scroll_offset > 0
}

TEST(RenderSliverToBoxAdapter, PartiallyVisibleAtBottom)
{
    cw::RenderSliverToBoxAdapter adapter;
    adapter.setChild(makeFixedHeightBox(100.0f));

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 30.0f; // budget runs out before the child's full 100px
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 100.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // child_extent - scroll > remaining
}

TEST(RenderSliverToBoxAdapter, HorizontalAxisUsesWidthAsExtent)
{
    cw::RenderSliverToBoxAdapter adapter;
    auto box = std::make_shared<cw::RenderSizedBox>();
    box->width = 80.0f;
    adapter.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::horizontal;
    c.cross_axis_extent      = 200.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 80.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 80.0f);
    EXPECT_FLOAT_EQ(box->size().width, 80.0f);
    EXPECT_FLOAT_EQ(box->size().height, 200.0f); // tight on cross axis
}

// ---------------------------------------------------------------------------
// 2. No-child case
// ---------------------------------------------------------------------------

TEST(RenderSliverToBoxAdapter, NoChildProducesAllZeroGeometry)
{
    cw::RenderSliverToBoxAdapter adapter;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    adapter.layoutSliver(c);

    const auto& g = adapter.geometry();
    EXPECT_EQ(g, cw::SliverGeometry{});
    EXPECT_EQ(adapter.child(), nullptr);
}

// ---------------------------------------------------------------------------
// 3. Real RenderViewport integration
// ---------------------------------------------------------------------------

TEST(RenderSliverToBoxAdapter, ViewportIntegrationReportsChildHeightAsScrollExtent)
{
    cw::RenderViewport vp;
    auto adapter = std::make_shared<cw::RenderSliverToBoxAdapter>();
    adapter->setChild(makeFixedHeightBox(400.0f));
    vp.insertChild(adapter, 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doViewportLayout(vp, 300.0f, 200.0f);

    EXPECT_FLOAT_EQ(controller->minScrollExtent(), 0.0f);
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 200.0f); // 400 - 200
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_TRUE(adapter->geometry().visible);
}

TEST(RenderSliverToBoxAdapter, ViewportIntegrationAtSeveralScrollPositions)
{
    cw::RenderViewport vp;
    auto adapter = std::make_shared<cw::RenderSliverToBoxAdapter>();
    adapter->setChild(makeFixedHeightBox(400.0f));
    vp.insertChild(adapter, 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doViewportLayout(vp, 300.0f, 200.0f); // establishes extents

    controller->jumpTo(150.0f);
    vp.markNeedsLayout();
    doViewportLayout(vp, 300.0f, 200.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -150.0f);
    EXPECT_TRUE(adapter->geometry().visible);
    EXPECT_FLOAT_EQ(adapter->geometry().paint_extent, 200.0f); // still fills the whole viewport

    controller->jumpTo(200.0f); // fully scrolled to the max extent
    vp.markNeedsLayout();
    doViewportLayout(vp, 300.0f, 200.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -200.0f);
    EXPECT_TRUE(adapter->geometry().visible);
    EXPECT_FLOAT_EQ(adapter->geometry().paint_extent, 200.0f); // last 200px of the 400px child
}

// ---------------------------------------------------------------------------
// 4. Multiple adapters stacked in one viewport
// ---------------------------------------------------------------------------

TEST(RenderSliverToBoxAdapter, MultipleAdaptersStackedInOneViewport)
{
    cw::RenderViewport vp;
    auto a0 = std::make_shared<cw::RenderSliverToBoxAdapter>();
    auto a1 = std::make_shared<cw::RenderSliverToBoxAdapter>();
    auto a2 = std::make_shared<cw::RenderSliverToBoxAdapter>();
    a0->setChild(makeFixedHeightBox(100.0f));
    a1->setChild(makeFixedHeightBox(150.0f));
    a2->setChild(makeFixedHeightBox(80.0f));
    vp.insertChild(a0, 0);
    vp.insertChild(a1, 1);
    vp.insertChild(a2, 2);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doViewportLayout(vp, 300.0f, 120.0f); // total content 330, viewport 120 -> max scroll 210

    EXPECT_FLOAT_EQ(controller->minScrollExtent(), 0.0f);
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 210.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 100.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(2), 250.0f);

    // Scroll so the second adapter is partially visible and the first is
    // fully scrolled past.
    controller->jumpTo(120.0f);
    vp.markNeedsLayout();
    doViewportLayout(vp, 300.0f, 120.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -120.0f);
    EXPECT_FALSE(a0->geometry().visible);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), -20.0f);
    EXPECT_TRUE(a1->geometry().visible);
    EXPECT_FLOAT_EQ(a1->geometry().paint_extent, 120.0f); // 150 - 20 scrolled into it, clamped to the full 120px viewport budget

    // a1 only consumed 100px of the 120px viewport budget (its own
    // layout_offset of -20 plus its 120px paint_extent lands at 100, not
    // 120) -- RenderViewport's own max_paint_offset bookkeeping (see
    // render_viewport.cpp's performLayout()) tracks *budget consumed*, not
    // "did this sibling's layout_offset cross the viewport edge", so a2
    // still receives the leftover 20px of paint budget even though its own
    // layout_offset (130) sits past the viewport's 120px extent.
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(2), 130.0f);
    EXPECT_TRUE(a2->geometry().visible);
    EXPECT_FLOAT_EQ(a2->geometry().paint_extent, 20.0f);
}
