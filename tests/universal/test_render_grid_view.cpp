#include <gtest/gtest.h>
#include <campello_widgets/ui/render_grid_view.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

static void doLayout(cw::RenderGridView& gv, float vw, float vh)
{
    gv.layout(cw::BoxConstraints::tight(vw, vh));
}

TEST(RenderGridView, PanGestureScrollsWithoutCompetingRecognizer)
{
    cw::RenderGridView gv;
    auto root = std::shared_ptr<cw::RenderBox>(&gv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    gv.item_count  = 40;
    gv.item_extent = 50.0f;
    doLayout(gv, 400.0f, 200.0f);
    gv.attach();

    EXPECT_EQ(gv.firstVisibleIndex(), 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // 100px up, exceeds 8px slop
    dispatcher.handlePointerEvent(move);

    EXPECT_NE(gv.firstVisibleIndex(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    gv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// Regresses the same uncontested-arena-must-not-skip-the-slop-gate bug fixed
// for RenderTreeView/RenderListView.
TEST(RenderGridView, SubSlopJitterDoesNotScrollEvenWhenUncontested)
{
    cw::RenderGridView gv;
    auto root = std::shared_ptr<cw::RenderBox>(&gv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    gv.item_count  = 40;
    gv.item_extent = 50.0f;
    doLayout(gv, 400.0f, 200.0f);
    gv.attach();

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dy : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {0.0f, 150.0f + dy};
        dispatcher.handlePointerEvent(move);
    }

    EXPECT_EQ(gv.firstVisibleIndex(), 0);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    gv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
