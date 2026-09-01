#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Reuses the production RenderSizedBox (no child, height set) as the
    // fixed-size RenderBox under test, mirroring
    // test_render_sliver_to_box_adapter.cpp's own fixture. Named distinctly
    // from that file's makeFixedHeightBox() -- Unity Build merges all test
    // .cpp files into one TU, so a same-named free function collides.
    std::shared_ptr<cw::RenderSizedBox> makeFixedHeightListBox(float height)
    {
        auto box = std::make_shared<cw::RenderSizedBox>();
        box->height = height;
        return box;
    }

    // Named distinctly from test_render_viewport.cpp's doLayout() and
    // test_render_sliver_to_box_adapter.cpp's doViewportLayout() -- Unity
    // Build merges all test .cpp files into one TU, so a same-named static
    // helper across files is a redefinition error (bit Stage 3 already).
    static void doFixedListViewportLayout(cw::RenderViewport& vp, float w, float h)
    {
        vp.layout(cw::BoxConstraints::tight(w, h));
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Standalone geometry correctness -- no items loaded, geometry must be
//    derivable purely from item_count/item_extent.
// ---------------------------------------------------------------------------

TEST(RenderSliverFixedExtentList, FullyVisible)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 5;
    list.item_extent = 50.0f; // content_extent = 250

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    list.layoutSliver(c);

    const auto& g = list.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.paint_origin, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.max_scroll_obstruction_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.hit_test_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.cache_extent, 250.0f);
    EXPECT_FALSE(g.scroll_offset_correction.has_value());
    EXPECT_TRUE(g.visible);
    EXPECT_FALSE(g.has_visual_overflow);
}

TEST(RenderSliverFixedExtentList, FullyScrolledPast)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 5;
    list.item_extent = 50.0f; // content_extent = 250

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 300.0f; // past the full 250px content
    c.remaining_paint_extent = 500.0f;
    list.layoutSliver(c);

    const auto& g = list.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.layout_extent, 0.0f);
    EXPECT_FLOAT_EQ(g.max_paint_extent, 250.0f);
    EXPECT_FALSE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow);
}

TEST(RenderSliverFixedExtentList, PartiallyVisibleInMiddle)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 5;
    list.item_extent = 50.0f; // content_extent = 250

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 40.0f; // top 40px scrolled past
    c.remaining_paint_extent = 100.0f; // budget runs out before the full 250px remainder
    list.layoutSliver(c);

    const auto& g = list.geometry();
    EXPECT_FLOAT_EQ(g.scroll_extent, 250.0f);
    EXPECT_FLOAT_EQ(g.paint_extent, 100.0f); // clamped to the 100px budget
    EXPECT_FLOAT_EQ(g.layout_extent, 100.0f);
    EXPECT_TRUE(g.visible);
    EXPECT_TRUE(g.has_visual_overflow); // scroll_offset > 0
}

TEST(RenderSliverFixedExtentList, ZeroItemCountOrZeroExtentProducesNoCrashAndZeroGeometry)
{
    {
        cw::RenderSliverFixedExtentList list;
        list.item_count  = 0;
        list.item_extent = 50.0f;

        cw::SliverConstraints c;
        c.axis                   = cw::Axis::vertical;
        c.cross_axis_extent      = 300.0f;
        c.remaining_paint_extent = 500.0f;
        list.layoutSliver(c);

        const auto& g = list.geometry();
        EXPECT_FLOAT_EQ(g.scroll_extent, 0.0f);
        EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
        EXPECT_FALSE(g.visible);
    }
    {
        cw::RenderSliverFixedExtentList list;
        list.item_count  = 5;
        list.item_extent = 0.0f;

        cw::SliverConstraints c;
        c.axis                   = cw::Axis::vertical;
        c.cross_axis_extent      = 300.0f;
        c.remaining_paint_extent = 500.0f;
        list.layoutSliver(c);

        const auto& g = list.geometry();
        EXPECT_FLOAT_EQ(g.scroll_extent, 0.0f);
        EXPECT_FLOAT_EQ(g.paint_extent, 0.0f);
        EXPECT_FALSE(g.visible);
    }
}

// ---------------------------------------------------------------------------
// 2. firstVisibleIndex()/lastVisibleIndex()
// ---------------------------------------------------------------------------

TEST(RenderSliverFixedExtentList, VisibleIndexRangeAtScrollStart)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 10;
    list.item_extent = 50.0f; // items at [0,50) [50,100) ... [450,500)

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 120.0f; // covers indices 0,1,2 (partially)
    list.layoutSliver(c);

    EXPECT_EQ(list.firstVisibleIndex(), 0);
    EXPECT_EQ(list.lastVisibleIndex(), 2);
}

TEST(RenderSliverFixedExtentList, VisibleIndexRangeScrolledIntoMiddle)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 10;
    list.item_extent = 50.0f;

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 130.0f; // into item 2 (item 2 spans [100,150))
    c.remaining_paint_extent = 100.0f;  // covers up to scroll+remaining=230 -> item 4
    list.layoutSliver(c);

    EXPECT_EQ(list.firstVisibleIndex(), 2);
    EXPECT_EQ(list.lastVisibleIndex(), 4);
}

TEST(RenderSliverFixedExtentList, VisibleIndexRangeClampsAtScrolledPastEnd)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 10;
    list.item_extent = 50.0f; // content_extent = 500

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 10000.0f; // way past the end
    c.remaining_paint_extent = 100.0f;
    list.layoutSliver(c);

    EXPECT_EQ(list.firstVisibleIndex(), 9); // clamped to item_count - 1
    EXPECT_EQ(list.lastVisibleIndex(), 9);
}

TEST(RenderSliverFixedExtentList, VisibleIndexRangeWithZeroItemCountOrExtent)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 0;
    list.item_extent = 50.0f;

    cw::SliverConstraints c;
    c.remaining_paint_extent = 100.0f;
    list.layoutSliver(c);

    EXPECT_EQ(list.firstVisibleIndex(), 0);
    EXPECT_EQ(list.lastVisibleIndex(), -1);
}

// ---------------------------------------------------------------------------
// 3. setItemBox()/removeItemBox() reparenting and layout of loaded children
// ---------------------------------------------------------------------------

TEST(RenderSliverFixedExtentList, SetItemBoxTwiceWithSameBoxIsNoOp)
{
    cw::RenderSliverFixedExtentList list;
    auto box = makeFixedHeightListBox(50.0f);

    list.setItemBox(0, box);
    EXPECT_EQ(list.itemBoxAt(0), box.get());
    EXPECT_EQ(box->parent(), &list);

    list.setItemBox(0, box); // same box again -- no-op, no reparent churn
    EXPECT_EQ(list.itemBoxAt(0), box.get());
    EXPECT_EQ(box->parent(), &list);
}

TEST(RenderSliverFixedExtentList, SetItemBoxReplacingUnparentsOldBox)
{
    cw::RenderSliverFixedExtentList list;
    auto box_a = makeFixedHeightListBox(50.0f);
    auto box_b = makeFixedHeightListBox(50.0f);

    list.setItemBox(0, box_a);
    EXPECT_EQ(box_a->parent(), &list);

    list.setItemBox(0, box_b);
    EXPECT_EQ(box_a->parent(), nullptr);
    EXPECT_EQ(box_b->parent(), &list);
    EXPECT_EQ(list.itemBoxAt(0), box_b.get());
}

TEST(RenderSliverFixedExtentList, RemoveItemBoxUnparentsAndErases)
{
    cw::RenderSliverFixedExtentList list;
    auto box = makeFixedHeightListBox(50.0f);

    list.setItemBox(2, box);
    EXPECT_NE(list.itemBoxAt(2), nullptr);

    list.removeItemBox(2);
    EXPECT_EQ(list.itemBoxAt(2), nullptr);
    EXPECT_EQ(box->parent(), nullptr);
}

TEST(RenderSliverFixedExtentList, OnlyLoadedBoxesAreLaidOutAtCorrectPositionAndConstraints)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 5;
    list.item_extent = 50.0f;

    auto box1 = makeFixedHeightListBox(50.0f);
    auto box3 = makeFixedHeightListBox(50.0f);
    list.setItemBox(1, box1);
    list.setItemBox(3, box3);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    list.layoutSliver(c);

    ASSERT_NE(box1->size().width, 0.0f); // laid out at all
    EXPECT_FLOAT_EQ(box1->size().width, 300.0f); // tight on cross axis
    EXPECT_FLOAT_EQ(box1->size().height, 50.0f); // tight on main axis (item_extent)
    EXPECT_FLOAT_EQ(box3->size().width, 300.0f);
    EXPECT_FLOAT_EQ(box3->size().height, 50.0f);
}

TEST(RenderSliverFixedExtentList, HorizontalAxisTightensWidthToItemExtent)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 3;
    list.item_extent = 40.0f;

    auto box = makeFixedHeightListBox(0.0f); // height irrelevant, tight constraints override it
    list.setItemBox(1, box);

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::horizontal;
    c.cross_axis_extent      = 200.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 500.0f;
    list.layoutSliver(c);

    EXPECT_FLOAT_EQ(box->size().width, 40.0f);  // tight on main axis
    EXPECT_FLOAT_EQ(box->size().height, 200.0f); // tight on cross axis
}

// ---------------------------------------------------------------------------
// 4. on_visible_range_changed firing
// ---------------------------------------------------------------------------

TEST(RenderSliverFixedExtentList, VisibleRangeChangedFiresOnlyWhenRangeActuallyChanges)
{
    cw::RenderSliverFixedExtentList list;
    list.item_count  = 20;
    list.item_extent = 50.0f;

    int fire_count = 0;
    list.on_visible_range_changed = [&] { ++fire_count; };

    cw::SliverConstraints c;
    c.axis                   = cw::Axis::vertical;
    c.cross_axis_extent      = 300.0f;
    c.scroll_offset           = 0.0f;
    c.remaining_paint_extent = 120.0f;
    list.layoutSliver(c); // first layout: cached range starts at (-1,-1), so this always fires
    EXPECT_EQ(fire_count, 1);

    // Re-run with identical constraints -- layoutSliver() itself no-ops on
    // unchanged constraints when not dirty, so force a fresh layout without
    // changing the visible index range.
    list.markNeedsLayout();
    list.layoutSliver(c);
    EXPECT_EQ(fire_count, 1); // range unchanged -- no additional fire

    // Now actually scroll far enough to change the visible range.
    c.scroll_offset = 500.0f;
    list.layoutSliver(c);
    EXPECT_EQ(fire_count, 2);
}

// ---------------------------------------------------------------------------
// 5. Real RenderViewport integration -- traces the confirmed no-extra-shift
//    paint formula end to end, not just geometry.
// ---------------------------------------------------------------------------

TEST(RenderSliverFixedExtentList, ViewportIntegrationReportsAggregateScrollExtent)
{
    cw::RenderViewport vp;
    auto list = std::make_shared<cw::RenderSliverFixedExtentList>();
    list->item_count  = 10;
    list->item_extent = 40.0f; // content_extent = 400
    vp.insertChild(list, 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doFixedListViewportLayout(vp, 300.0f, 200.0f);

    EXPECT_FLOAT_EQ(controller->minScrollExtent(), 0.0f);
    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 200.0f); // 400 - 200
    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), 0.0f);
    EXPECT_TRUE(list->geometry().visible);
}

TEST(RenderSliverFixedExtentList, ViewportIntegrationItemPaintOffsetComposesWithLayoutOffset)
{
    // Confirms the plan's central formula: RenderViewport shifts the whole
    // sliver by -scroll_offset via layout_offset, and this class paints each
    // item at a plain offset + item_pos with no additional shift -- so an
    // item's absolute, viewport-relative paint position must equal
    // (vp.layoutOffsetAt(sliver_index) + idx * item_extent), traced through
    // both layers rather than asserted from geometry alone.
    cw::RenderViewport vp;
    auto list = std::make_shared<cw::RenderSliverFixedExtentList>();
    list->item_count  = 10;
    list->item_extent = 40.0f; // items at [0,40) [40,80) [80,120) ...

    auto box2 = makeFixedHeightListBox(40.0f); // item index 2, spans [80,120)
    list->setItemBox(2, box2);
    vp.insertChild(list, 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doFixedListViewportLayout(vp, 300.0f, 200.0f); // establishes extents

    // At scroll 0: item 2's expected viewport-relative top = layoutOffsetAt(0) + 2*40 = 0 + 80.
    {
        list->markNeedsLayout();
        doFixedListViewportLayout(vp, 300.0f, 200.0f);
        const float expected_item_top = vp.layoutOffsetAt(0) + 2.0f * list->item_extent;
        EXPECT_FLOAT_EQ(expected_item_top, 80.0f);
    }

    // Scroll 60px into the list -- the whole sliver's layout_offset becomes
    // -60 (RenderViewport's own shift), and item 2's own content-relative
    // position (80) is unchanged, so its new viewport-relative top must be
    // exactly layoutOffsetAt(0) + 80 = -60 + 80 = 20, NOT 80 - 60 applied
    // twice (which double-shifting would produce, e.g. -40).
    controller->jumpTo(60.0f);
    list->markNeedsLayout();
    doFixedListViewportLayout(vp, 300.0f, 200.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -60.0f);
    const float expected_item_top_after_scroll = vp.layoutOffsetAt(0) + 2.0f * list->item_extent;
    EXPECT_FLOAT_EQ(expected_item_top_after_scroll, 20.0f);

    // The item's OWN local offset (as stored/used by performPaint) must
    // still be its unshifted content-relative position (idx * item_extent),
    // confirming the class itself never subtracts scroll -- only the parent
    // viewport's layout_offset carries the shift.
    ASSERT_NE(list->itemBoxAt(2), nullptr);
}

TEST(RenderSliverFixedExtentList, MultipleItemsAtSeveralScrollPositions)
{
    cw::RenderViewport vp;
    auto list = std::make_shared<cw::RenderSliverFixedExtentList>();
    list->item_count  = 6;
    list->item_extent = 50.0f; // content_extent = 300

    for (int i = 0; i < 6; ++i)
        list->setItemBox(i, makeFixedHeightListBox(50.0f));

    vp.insertChild(list, 0);

    auto controller = std::make_shared<cw::ScrollController>();
    vp.setController(controller);

    doFixedListViewportLayout(vp, 300.0f, 120.0f); // viewport shows ~2.4 items; max scroll = 300-120=180

    EXPECT_FLOAT_EQ(controller->maxScrollExtent(), 180.0f);
    EXPECT_EQ(list->firstVisibleIndex(), 0);
    EXPECT_EQ(list->lastVisibleIndex(), 2);

    controller->jumpTo(180.0f); // fully scrolled to the end
    list->markNeedsLayout();
    doFixedListViewportLayout(vp, 300.0f, 120.0f);

    EXPECT_FLOAT_EQ(vp.layoutOffsetAt(0), -180.0f);
    EXPECT_EQ(list->lastVisibleIndex(), 5); // clamped to item_count - 1
    EXPECT_TRUE(list->geometry().visible);
    EXPECT_FLOAT_EQ(list->geometry().paint_extent, 120.0f); // still fills the whole viewport budget
}
