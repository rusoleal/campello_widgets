#include <gtest/gtest.h>
#include <campello_widgets/ui/render_page_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    struct PageViewFixture : public ::testing::Test
    {
        cw::RenderPageView pv;
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        void SetUp() override
        {
            pv.scroll_direction = cw::Axis::horizontal;
            for (int i = 0; i < 5; ++i)
            {
                auto page = std::make_shared<cw::RenderSizedBox>();
                pv.insertChild(page, i);
            }

            auto root = std::shared_ptr<cw::RenderBox>(&pv, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            pv.layout(cw::BoxConstraints::tight(400.0f, 400.0f));
            pv.attach();
        }

        void TearDown() override
        {
            pv.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(PageViewFixture, PanGestureSwipesPageWithoutCompetingRecognizer)
{
    EXPECT_EQ(pv.currentPage(), 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {300.0f, 200.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 200.0f}; // 300px left, exceeds 8px slop, more than half a page
    dispatcher->handlePointerEvent(move);

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = {0.0f, 200.0f};
    dispatcher->handlePointerEvent(up);

    EXPECT_EQ(pv.currentPage(), 1);
}

// Regresses the same uncontested-arena-must-not-skip-the-slop-gate bug fixed
// for RenderTreeView/RenderListView/RenderGridView/RenderSingleChildScrollView.
TEST_F(PageViewFixture, SubSlopJitterDoesNotSwipeEvenWhenUncontested)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {300.0f, 200.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dx : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {300.0f + dx, 200.0f};
        dispatcher->handlePointerEvent(move);
    }

    EXPECT_EQ(pv.currentPage(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}
