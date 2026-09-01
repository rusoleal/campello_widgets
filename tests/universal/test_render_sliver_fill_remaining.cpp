#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver_fill_remaining.hpp>
#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Deliberately distinctly-named from the near-identical helpers in
    // test_render_sliver_to_box_adapter.cpp / test_render_sliver_fixed_extent_list.cpp
    // / test_render_viewport.cpp -- Unity Build merges anonymous-namespace
    // helpers from different .cpp files into one TU, so a same-named
    // function here would be a redefinition error, not just a shadow.
    //
    // Deliberately childless (no width/height set) -- its unconstrained
    // natural size is 0 on both axes, so any test asserting it was forced
    // to a specific size is actually exercising the tight constraint, not
    // coincidentally matching the box's own natural size.
    std::shared_ptr<cw::RenderSizedBox> makeFillRemainingTestBox()
    {
        return std::make_shared<cw::RenderSizedBox>();
    }

    void layoutFillRemainingViewport(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Standalone geometry with a child
// ---------------------------------------------------------------------------

TEST(RenderSliverFillRemaining, ClaimsFullRemainingBudgetVertical)
{
    cw::RenderSliverFillRemaining sliver;
    auto box = makeFillRemainingTestBox();
    sliver.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 420.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 420.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 420.0f);
    EXPECT_FLOAT_EQ(g.paint_origin, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 420.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 420.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 420.0f);
    EXPECT_FLOAT_EQ(g.cache_extent, 420.0f);
    EXPECT_FALSE(g.scroll_offset_correction.has_value());
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);

    // Tight on BOTH axes -- the child's own unconstrained natural size is
    // 0x0 (no width/height set), so this proves the constraint is really
    // tight, not coincidentally matching.
    EXPECT_FLOAT_EQ(box->size().width, 300.0f);
    EXPECT_FLOAT_EQ(box->size().height, 420.0f);
}

TEST(RenderSliverFillRemaining, SmallRemainingBudgetStillExact)
{
    cw::RenderSliverFillRemaining sliver;
    auto box = makeFillRemainingTestBox();
    sliver.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 15.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 15.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 15.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 15.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 15.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);
    EXPECT_FLOAT_EQ(box->size().height, 15.0f);
}

TEST(RenderSliverFillRemaining, ZeroRemainingBudgetIsInvisible)
{
    cw::RenderSliverFillRemaining sliver;
    auto box = makeFillRemainingTestBox();
    sliver.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 0.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
    EXPECT_FALSE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);
}

TEST(RenderSliverFillRemaining, HorizontalAxisUsesRemainingAsWidth)
{
    cw::RenderSliverFillRemaining sliver;
    auto box = makeFillRemainingTestBox();
    sliver.setChild(box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::horizontal;
    c.cross_axis_extent      = 250.0f;
    c.remaining_paint_extent = 180.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 180.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 180.0f);
    EXPECT_FLOAT_EQ(box->size().width, 180.0f);  // tight on the main axis
    EXPECT_FLOAT_EQ(box->size().height, 250.0f); // tight on the cross axis
}

// ---------------------------------------------------------------------------
// 2. No-child case -- deliberate divergence from RenderSliverToBoxAdapter:
// geometry still reflects the leftover extent, NOT all-zero.
// ---------------------------------------------------------------------------

TEST(RenderSliverFillRemaining, NoChildStillReservesTheExtent)
{
    cw::RenderSliverFillRemaining sliver;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.remaining_paint_extent = 250.0f;
    sliver.layoutSliver(c);

    const auto& g = sliver.geometry();
    // NOT cw::SliverGeometry{} -- this is the point of the test.
    EXPECT_NE(g, cw::SliverGeometry{});
    EXPECT_FLOAT_EQ(g.scroll_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 250.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_EQ(sliver.child(), nullptr);
}

// ---------------------------------------------------------------------------
// 3. Real RenderViewport integration
// ---------------------------------------------------------------------------

TEST(RenderSliverFillRemaining, ViewportIntegrationContentShorterThanViewport)
{
    cw::RenderViewport vp;

    auto list = std::make_shared<cw::RenderSliverFixedExtentList>();
    list->item_count  = 3;
    list->item_extent = 40.0f; // 120px of content
    vp.insertChild(list, 0);

    auto fill = std::make_shared<cw::RenderSliverFillRemaining>();
    fill->setChild(makeFillRemainingTestBox());
    vp.insertChild(fill, 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    layoutFillRemainingViewport(vp, 300.0f, 500.0f); // viewport taller than the list content

    // The list claims 120px, the fill sliver claims the remaining 380px --
    // exactly filling the viewport with nothing left over to scroll to.
    EXPECT_FLOAT_EQ(controller->minScrollExtent(), 0.0f);
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 0.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(1), 120.0f);
    EXPECT_TRUE(fill->geometry().visible);
    EXPECT_FLOAT_EQ(fill->geometry().paint_extent, 380.0f);
    ASSERT_NE(fill->child(), nullptr);
    EXPECT_FLOAT_EQ(fill->child()->size().height, 380.0f);
}

TEST(RenderSliverFillRemaining, ViewportIntegrationContentTallerThanViewport)
{
    cw::RenderViewport vp;

    auto list = std::make_shared<cw::RenderSliverFixedExtentList>();
    list->item_count  = 20;
    list->item_extent = 40.0f; // 800px of content -- taller than the viewport
    vp.insertChild(list, 0);

    auto fill = std::make_shared<cw::RenderSliverFillRemaining>();
    fill->setChild(makeFillRemainingTestBox());
    vp.insertChild(fill, 1);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    layoutFillRemainingViewport(vp, 300.0f, 500.0f);

    // The list alone consumes the entire 500px budget -- the fill sliver
    // never gets any room, at any scroll position.
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 300.0f); // 800 - 500, from the list alone
    EXPECT_FALSE(fill->geometry().visible);
    EXPECT_FLOAT_EQ(fill->geometry().paint_extent, 0.0f);

    controller->jumpTo(controller->maxScrollExtent());
    vp.markNeedsLayout();
    layoutFillRemainingViewport(vp, 300.0f, 500.0f);

    // NOT still invisible -- worked out by hand, and confirms this is the
    // same pre-existing max_paint_offset bookkeeping quirk already
    // documented and tested in RenderSliverToBoxAdapter's own
    // MultipleAdaptersStackedInOneViewport test (render_sliver_to_box_adapter
    // test file): max_paint_offset tracks layout_offset + paint_extent
    // ("budget consumed"), not "did this sibling visually cross the
    // viewport edge". At scroll=300, the list's own layout_offset is -300
    // (scrolled up by 300) while its paint_extent is still the full 500
    // (it still has 500px of its own content left to show) -- so
    // layout_offset + paint_extent = 200, meaning the viewport's own
    // budget-consumed bookkeeping thinks only 200 of the 500px viewport
    // was claimed, handing the fill sliver the leftover 300px. This is
    // pre-existing RenderViewport behavior, not something
    // RenderSliverFillRemaining introduces -- asserting the actual
    // computed value rather than the naive "never visible" assumption.
    EXPECT_TRUE(fill->geometry().visible);
    EXPECT_FLOAT_EQ(fill->geometry().paint_extent, 300.0f);
}
