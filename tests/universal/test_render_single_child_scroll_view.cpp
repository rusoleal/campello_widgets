#include <gtest/gtest.h>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    struct ScrollViewFixture : public ::testing::Test
    {
        cw::RenderSingleChildScrollView sv;
        std::shared_ptr<cw::ScrollController> controller = std::make_shared<cw::ScrollController>();
        std::shared_ptr<cw::PointerDispatcher> dispatcher;

        void SetUp() override
        {
            sv.scroll_axis = cw::Axis::vertical;
            sv.setController(controller);

            auto child = std::make_shared<cw::RenderSizedBox>();
            child->width  = 400.0f;
            child->height = 2000.0f; // plenty of scroll room
            sv.setChild(child);

            auto root = std::shared_ptr<cw::RenderBox>(&sv, [](cw::RenderBox*) {});
            dispatcher = std::make_shared<cw::PointerDispatcher>(root);
            cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

            sv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
            sv.attach();
        }

        void TearDown() override
        {
            sv.detach();
            cw::PointerDispatcher::setActiveDispatcher(nullptr);
        }
    };
} // namespace

TEST_F(ScrollViewFixture, PanGestureScrollsWithoutCompetingRecognizer)
{
    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {0.0f, 50.0f}; // 100px up, exceeds 8px slop
    dispatcher->handlePointerEvent(move);

    EXPECT_GT(controller->offset(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}

// Regresses the same uncontested-arena-must-not-skip-the-slop-gate bug fixed
// for RenderTreeView/RenderListView/RenderGridView.
TEST_F(ScrollViewFixture, SubSlopJitterDoesNotScrollEvenWhenUncontested)
{
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 150.0f};
    dispatcher->handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dy : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {0.0f, 150.0f + dy};
        dispatcher->handlePointerEvent(move);
    }

    EXPECT_FLOAT_EQ(controller->offset(), 0.0f);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher->handlePointerEvent(up);
}
