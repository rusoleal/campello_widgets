#include <gtest/gtest.h>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_grid_view.hpp>
#include <campello_widgets/ui/render_page_view.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace cw = systems::leal::campello_widgets;

// Stage 0b regression coverage: RenderSingleChildScrollView, RenderListView,
// RenderGridView and RenderPageView each now self-boundary their own paint
// output via OffsetLayer (mirroring Flutter's RenderViewport.isRepaintBoundary
// — see TODO.md's "Paint/Compositing Architecture" section). The underlying
// caching mechanics (offset tracking, absolute-clip-rect correctness) are
// already exhaustively covered by test_render_repaint_boundary.cpp and
// test_offset_layer.cpp; these tests only verify each scrollable's paint()
// is actually wired to its own OffsetLayer instead of the base
// RenderObject::paint()'s unconditional re-walk.
//
// Stage 0d note: unlike RenderRepaintBoundary, these four always clip their
// own content to their own viewport in performPaint() — so their cached
// picture always contains a PushClipRectCmd. PictureLayer no longer flags
// PushClipRectCmd as unsafe (OffsetLayer::maybeReplay() shifts its stored
// rect by hand on reposition — see shiftClipRects()), so repositioning one
// of these now takes the cheap delta-translate path too, same as any other
// clipped content. This is the scroll/resize performance fix's actual
// payoff: previously every scroll delta forced a full re-record of the
// scrollable's entire visible content.

namespace
{
    // Same helper as test_render_repaint_boundary.cpp: counts how many times
    // performPaint() actually runs.
    class ScrollableCountingRenderBox : public cw::RenderBox
    {
    public:
        int paintCount = 0;

        void performLayout() override
        {
            size_ = constraints_.constrain(cw::Size{40.0f, 30.0f});
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            ++paintCount;
            context.canvas().drawRect(
                cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                cw::Paint::filled(cw::Color::black()));
        }
    };
} // namespace

TEST(RenderSingleChildScrollViewPaintCache, CleanRepaintReplaysCacheWithoutWalkingChild)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderSingleChildScrollView view;
    view.setChild(child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "clean scroll view should replay its cache, not re-walk its child";
    EXPECT_FALSE(ctx2.commands().empty());
}

TEST(RenderSingleChildScrollViewPaintCache, MarkNeedsPaintForcesReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderSingleChildScrollView view;
    view.setChild(child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    view.markNeedsPaint();
    ASSERT_TRUE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2)
        << "dirtying the scroll view (e.g. a scroll delta) should force a fresh recording";
}

TEST(RenderListViewPaintCache, CleanRepaintReplaysCacheWithoutWalkingChildren)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderListView view;
    view.item_count  = 5;
    view.item_extent = 50.0f;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "clean list view should replay its cache, not re-walk its items";
}

TEST(RenderListViewPaintCache, MarkNeedsPaintForcesReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderListView view;
    view.item_count  = 5;
    view.item_extent = 50.0f;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    view.markNeedsPaint();
    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2)
        << "dirtying the list view (e.g. a scroll delta) should force a fresh recording";
}

TEST(RenderGridViewPaintCache, CleanRepaintReplaysCacheWithoutWalkingChildren)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderGridView view;
    view.item_count       = 6;
    view.item_extent      = 50.0f;
    view.cross_axis_count = 2;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "clean grid view should replay its cache, not re-walk its items";
}

TEST(RenderGridViewPaintCache, MarkNeedsPaintForcesReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderGridView view;
    view.item_count       = 6;
    view.item_extent      = 50.0f;
    view.cross_axis_count = 2;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    view.markNeedsPaint();
    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2)
        << "dirtying the grid view (e.g. a scroll delta) should force a fresh recording";
}

TEST(RenderPageViewPaintCache, CleanRepaintReplaysCacheWithoutWalkingChildren)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderPageView view;
    view.insertChild(child, 0);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "clean page view should replay its cache, not re-walk its pages";
}

TEST(RenderPageViewPaintCache, MarkNeedsPaintForcesReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderPageView view;
    view.insertChild(child, 0);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    view.markNeedsPaint();
    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2)
        << "dirtying the page view (e.g. a page-drag delta) should force a fresh recording";
}

TEST(RenderSingleChildScrollViewPaintCache, RepositionShiftsOwnViewportClipWithoutReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderSingleChildScrollView view;
    view.setChild(child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(view.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{50.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "a scroll view's own viewport clip is a PushClipRectCmd, which is cheaply "
           "repositionable — reposition must not force a full re-record";
}

TEST(RenderListViewPaintCache, RepositionShiftsOwnViewportClipWithoutReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderListView view;
    view.item_count  = 5;
    view.item_extent = 50.0f;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 40.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "a list view's own viewport clip is cheaply repositionable — reposition "
           "must not force a full re-record";
}

TEST(RenderGridViewPaintCache, RepositionShiftsOwnViewportClipWithoutReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderGridView view;
    view.item_count       = 6;
    view.item_extent      = 50.0f;
    view.cross_axis_count = 2;
    view.setItemBox(0, child);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{0.0f, 40.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "a grid view's own viewport clip is cheaply repositionable — reposition "
           "must not force a full re-record";
}

TEST(RenderPageViewPaintCache, RepositionShiftsOwnViewportClipWithoutReRecording)
{
    auto child = std::make_shared<ScrollableCountingRenderBox>();
    cw::RenderPageView view;
    view.insertChild(child, 0);
    view.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    view.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    view.paint(ctx2, cw::Offset{40.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1)
        << "a page view's own viewport clip is cheaply repositionable — reposition "
           "must not force a full re-record";
}
