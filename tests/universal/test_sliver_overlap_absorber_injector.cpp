#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver_overlap_absorber.hpp>
#include <campello_widgets/ui/render_sliver_overlap_injector.hpp>
#include <campello_widgets/ui/render_sliver_persistent_header.hpp>
#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Deliberately distinctly-named from the near-identical helpers in
    // test_render_sliver_to_box_adapter.cpp / test_render_sliver_fixed_extent_list.cpp
    // / test_render_sliver_fill_remaining.cpp -- Unity Build merges
    // anonymous-namespace helpers from different .cpp files into one TU, so
    // a same-named function here would be a redefinition error, not just a
    // shadow.
    void layoutOverlapViewport(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }

    std::shared_ptr<cw::RenderSliverPersistentHeader> makePinnedHeader(float min_extent, float max_extent)
    {
        auto header = std::make_shared<cw::RenderSliverPersistentHeader>();
        header->min_extent = min_extent;
        header->max_extent = max_extent;
        return header;
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Absorber, standalone
// ---------------------------------------------------------------------------

TEST(RenderSliverOverlapAbsorber, ForwardsChildGeometryUnchangedAndWritesHandleAtScrollZero)
{
    cw::RenderSliverOverlapAbsorber absorber;
    absorber.setChild(makePinnedHeader(60.0f, 160.0f));
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    absorber.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    absorber.layoutSliver(c);

    // Header laid out directly with the same constraints, for comparison.
    auto reference_header = makePinnedHeader(60.0f, 160.0f);
    reference_header->layoutSliver(c);

    EXPECT_EQ(absorber.geometry(), reference_header->geometry());
    EXPECT_FLOAT_EQ(handle->layout_extent, 60.0f); // min_extent, the resting obstruction
}

TEST(RenderSliverOverlapAbsorber, HandleStaysAtMinExtentRegardlessOfCollapseState)
{
    cw::RenderSliverOverlapAbsorber absorber;
    absorber.setChild(makePinnedHeader(60.0f, 160.0f));
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    absorber.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 500.0f; // far past the header's full 160px expanded height
    c.remaining_paint_extent = 500.0f;
    absorber.layoutSliver(c);

    // max_scroll_obstruction_extent (and thus the handle) is min_extent
    // constant regardless of collapse phase -- confirmed in
    // RenderSliverPersistentHeader::performLayoutSliver() (Stage 5).
    EXPECT_FLOAT_EQ(absorber.geometry().max_scroll_obstruction_extent, 60.0f);
    EXPECT_FLOAT_EQ(handle->layout_extent, 60.0f);
}

// ---------------------------------------------------------------------------
// 2. Absorber, no child
// ---------------------------------------------------------------------------

TEST(RenderSliverOverlapAbsorber, NoChildProducesAllZeroGeometryAndResetsHandle)
{
    cw::RenderSliverOverlapAbsorber absorber;
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    handle->layout_extent = 42.0f; // pre-existing value, should be reset to 0
    absorber.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    absorber.layoutSliver(c);

    EXPECT_EQ(absorber.geometry(), cw::SliverGeometry{});
    EXPECT_FLOAT_EQ(handle->layout_extent, 0.0f);
    EXPECT_EQ(absorber.child(), nullptr);
}

// ---------------------------------------------------------------------------
// 3. Absorber, null handle
// ---------------------------------------------------------------------------

TEST(RenderSliverOverlapAbsorber, NullHandleDoesNotCrashAndStillForwardsGeometry)
{
    cw::RenderSliverOverlapAbsorber absorber;
    absorber.setChild(makePinnedHeader(60.0f, 160.0f));
    // absorber.handle left null deliberately.

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    absorber.layoutSliver(c);

    EXPECT_FLOAT_EQ(absorber.geometry().max_scroll_obstruction_extent, 60.0f);
    EXPECT_FLOAT_EQ(absorber.geometry().paint_extent, 160.0f);
}

// ---------------------------------------------------------------------------
// 4. Injector, standalone -- mirrors RenderSliverToBoxAdapter's own 4 cases
// ---------------------------------------------------------------------------

TEST(RenderSliverOverlapInjector, FullyVisible)
{
    cw::RenderSliverOverlapInjector injector;
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    handle->layout_extent = 100.0f;
    injector.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    injector.layoutSliver(c);

    const auto& g = injector.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_origin, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 0.0f); // pure spacer -- nothing to hit, unlike ToBoxAdapter
    EXPECT_FLOAT_EQ(g.cache_extent, 100.0f);
    EXPECT_FALSE(g.scroll_offset_correction.has_value());
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);
}

TEST(RenderSliverOverlapInjector, FullyScrolledPast)
{
    cw::RenderSliverOverlapInjector injector;
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    handle->layout_extent = 100.0f;
    injector.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 150.0f; // past the gap's full 100px extent
    c.remaining_paint_extent = 500.0f;
    injector.layoutSliver(c);

    const auto& g = injector.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 0.0f);
    EXPECT_FALSE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow);
}

TEST(RenderSliverOverlapInjector, PartiallyVisibleAtTop)
{
    cw::RenderSliverOverlapInjector injector;
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    handle->layout_extent = 100.0f;
    injector.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 40.0f;
    c.remaining_paint_extent = 500.0f;
    injector.layoutSliver(c);

    const auto& g = injector.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 60.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 60.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 0.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow);
}

TEST(RenderSliverOverlapInjector, PartiallyVisibleAtBottom)
{
    cw::RenderSliverOverlapInjector injector;
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();
    handle->layout_extent = 100.0f;
    injector.handle = handle;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 30.0f;
    injector.layoutSliver(c);

    const auto& g = injector.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 100.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 30.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 0.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow);
}

// ---------------------------------------------------------------------------
// 5. Injector, null handle
// ---------------------------------------------------------------------------

TEST(RenderSliverOverlapInjector, NullHandleTreatedAsZeroExtentAndDoesNotCrash)
{
    cw::RenderSliverOverlapInjector injector;
    // injector.handle left null deliberately.

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 500.0f;
    injector.layoutSliver(c);

    const auto& g = injector.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
    EXPECT_FALSE(g.visible);
}

// ---------------------------------------------------------------------------
// 6. Real two-viewport integration
// ---------------------------------------------------------------------------

TEST(SliverOverlapAbsorberInjector, InnerViewportReservesGapMatchingOuterHeaderObstruction)
{
    auto handle = std::make_shared<cw::SliverOverlapAbsorberHandle>();

    // Outer viewport: an absorber wrapping a pinned header.
    cw::RenderViewport outer;
    auto absorber = std::make_shared<cw::RenderSliverOverlapAbsorber>();
    absorber->setChild(makePinnedHeader(60.0f, 160.0f));
    absorber->handle = handle;
    outer.insertChild(absorber, 0);

    auto outer_controller = std::make_shared<cw::ScrollController>();
    outer.setController(outer_controller);

    layoutOverlapViewport(outer, 300.0f, 500.0f);
    EXPECT_FLOAT_EQ(handle->layout_extent, 60.0f);

    // Inner viewport: an injector sharing the same handle, followed by a
    // fixed-extent list body.
    cw::RenderViewport inner;
    auto injector = std::make_shared<cw::RenderSliverOverlapInjector>();
    injector->handle = handle;
    inner.insertChild(injector, 0);

    auto body = std::make_shared<cw::RenderSliverFixedExtentList>();
    body->item_count  = 20;
    body->item_extent = 40.0f;
    inner.insertChild(body, 1);

    auto inner_controller = std::make_shared<cw::ScrollController>();
    inner.setController(inner_controller);

    layoutOverlapViewport(inner, 300.0f, 500.0f);

    // The injector reserves exactly 60px -- the body starts below where the
    // outer header will rest once pinned, not at the inner viewport's own
    // top edge.
    EXPECT_TRUE(injector->geometry().visible);
    EXPECT_FLOAT_EQ(injector->geometry().paint_extent, 60.0f);
    EXPECT_FLOAT_EQ(inner.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(inner.layoutOffsetAt(1), 60.0f);
}
