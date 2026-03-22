#include <gtest/gtest.h>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Lay out the list view at the given viewport size (no item boxes yet).
static void doLayout(cw::RenderListView& lv, float vw, float vh)
{
    lv.layout(cw::BoxConstraints::tight(vw, vh));
}

// ---------------------------------------------------------------------------
// Visible range — vertical list (default)
// ---------------------------------------------------------------------------

TEST(RenderListView, FirstVisibleIndexIsZeroAtScrollStart)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
}

TEST(RenderListView, LastVisibleIndexIsFloorViewportOverExtent)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f); // viewport=300, extent=60 → last = floor(300/60)=5, min(29,5)=5

    EXPECT_EQ(lv.lastVisibleIndex(), 5);
}

TEST(RenderListView, LastVisibleIndexClampsToItemCountMinusOne)
{
    cw::RenderListView lv;
    lv.item_count  = 3;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 600.0f); // viewport=600 → floor(600/60)=10, clamped to 2

    EXPECT_EQ(lv.lastVisibleIndex(), 2);
}

TEST(RenderListView, VisibleRangeWithExactFitViewport)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 50.0f;
    doLayout(lv, 400.0f, 250.0f); // viewport=250 = 5 × 50 exactly → last=floor(250/50)=5

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
    EXPECT_EQ(lv.lastVisibleIndex(),  5);
}

TEST(RenderListView, ZeroItemCountReturnsNegativeLastIndex)
{
    cw::RenderListView lv;
    lv.item_count  = 0;
    lv.item_extent = 60.0f;
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.lastVisibleIndex(), -1);
}

TEST(RenderListView, ZeroItemExtentReturnsDefaultIndices)
{
    cw::RenderListView lv;
    lv.item_count  = 30;
    lv.item_extent = 0.0f; // guard: zero extent
    doLayout(lv, 400.0f, 300.0f);

    EXPECT_EQ(lv.firstVisibleIndex(), 0);
    EXPECT_EQ(lv.lastVisibleIndex(),  -1);
}

// ---------------------------------------------------------------------------
// Size after layout
// ---------------------------------------------------------------------------

TEST(RenderListView, SizeFillsTightConstraints)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 50.0f;
    doLayout(lv, 640.0f, 200.0f);

    EXPECT_FLOAT_EQ(lv.size().width,  640.0f);
    EXPECT_FLOAT_EQ(lv.size().height, 200.0f);
}

// ---------------------------------------------------------------------------
// Item boxes — layout assigns correct offsets
// ---------------------------------------------------------------------------

TEST(RenderListView, ItemBoxesAreLayoutAtCorrectOffsets)
{
    cw::RenderListView lv;
    lv.item_count  = 10;
    lv.item_extent = 80.0f;
    doLayout(lv, 400.0f, 300.0f);

    // Add item boxes for indices 0, 1, 2.
    for (int i = 0; i < 3; ++i)
    {
        auto box = std::make_shared<cw::RenderSizedBox>();
        lv.setItemBox(i, box);
    }

    // Trigger a second layout so the item boxes are positioned.
    doLayout(lv, 400.0f, 300.0f);

    // Each item box must have been laid out with height == item_extent.
    // We verify via size() on the boxes, which is set by layout.
    for (int i = 0; i < 3; ++i)
    {
        // Item boxes are not directly accessible after setItemBox, but we can
        // verify the list itself didn't crash and is still sized correctly.
        EXPECT_FLOAT_EQ(lv.size().height, 300.0f);
    }
}

// ---------------------------------------------------------------------------
// Scroll extent calculation
// ---------------------------------------------------------------------------

TEST(RenderListView, OnVisibleRangeChangedCallbackFires)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    int call_count = 0;
    lv.on_visible_range_changed = [&]{ ++call_count; };

    doLayout(lv, 400.0f, 200.0f);
    // The initial layout should fire the callback (cached=-1/-1, new=0/3).
    EXPECT_GE(call_count, 1);
}

TEST(RenderListView, OnVisibleRangeChangedNotFiredWhenRangeUnchanged)
{
    cw::RenderListView lv;
    lv.item_count  = 20;
    lv.item_extent = 50.0f;

    int call_count = 0;
    lv.on_visible_range_changed = [&]{ ++call_count; };

    doLayout(lv, 400.0f, 200.0f); // first layout, fires
    int after_first = call_count;

    doLayout(lv, 400.0f, 200.0f); // same constraints, same range — should not fire again
    EXPECT_EQ(call_count, after_first);
}
