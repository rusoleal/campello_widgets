#include <gtest/gtest.h>
#include <campello_widgets/ui/render_slider.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(RenderSlider, DragUpdatesValueWithoutCompetingRecognizer)
{
    cw::RenderSlider slider;
    int change_count = 0;
    slider.on_value_changed = [&](float v) { slider.value = v; ++change_count; };

    auto root = std::shared_ptr<cw::RenderBox>(&slider, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    slider.layout(cw::BoxConstraints::tight(200.0f, 40.0f));
    slider.attach();

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {0.0f, 20.0f};
    dispatcher.handlePointerEvent(down);

    EXPECT_EQ(change_count, 1); // slider claims and reacts immediately, no slop

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {100.0f, 20.0f};
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(change_count, 2);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    slider.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// Regresses the "ad hoc, no arbitration" class of bug for non-slop direct-
// manipulation widgets: a slider nested inside a scrollable list should
// reliably win the drag (it claims on pointer-down, before the list's own
// slop check ever runs), not have both react simultaneously.
TEST(RenderSlider, NestedInListDoesNotLetListScrollWhileDraggingThumb)
{
    cw::RenderListView list;
    list.scroll_axis = cw::Axis::vertical;
    list.item_count  = 10;
    list.item_extent = 100.0f;

    cw::RenderSlider slider;
    int change_count = 0;
    slider.on_value_changed = [&](float v) { slider.value = v; ++change_count; };

    auto slider_box = std::shared_ptr<cw::RenderBox>(&slider, [](cw::RenderBox*) {});
    list.setItemBox(0, slider_box);

    auto root = std::shared_ptr<cw::RenderBox>(&list, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    list.layout(cw::BoxConstraints::tight(400.0f, 400.0f));
    list.attach();
    slider.attach();

    EXPECT_EQ(list.firstVisibleIndex(), 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {100.0f, 50.0f}; // inside item 0 / the slider
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {200.0f, -50.0f}; // big diagonal jump
    dispatcher.handlePointerEvent(move);

    EXPECT_GE(change_count, 1);
    EXPECT_EQ(list.firstVisibleIndex(), 0); // list never got a chance to claim

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    slider.detach();
    list.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
