#include <gtest/gtest.h>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/render_mouse_region.hpp>
#include <campello_widgets/ui/render_stack.hpp>

#include <optional>
#include <vector>

namespace cw = systems::leal::campello_widgets;

// on_hover reports the pointer in the region's own coordinates (origin at
// its top-left corner), as GestureDetector does, not the window's: a region
// away from the window's corner (e.g. a viewport beside a side panel) must
// not see its hover points shifted by its offset.
TEST(RenderMouseRegion, HoverPositionIsLocalToTheRegion)
{
    cw::RenderStack stack;
    cw::RenderMouseRegion region;
    auto region_box = std::shared_ptr<cw::RenderBox>(&region, [](cw::RenderBox*) {});
    stack.insertChild(region_box, 0, 50.0f, 30.0f, std::nullopt, std::nullopt, 100.0f, 60.0f);

    auto root = std::shared_ptr<cw::RenderBox>(&stack, [](cw::RenderBox*) {});
    auto dispatcher = std::make_shared<cw::PointerDispatcher>(root);
    cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());
    stack.layout(cw::BoxConstraints::tight(300.0f, 200.0f));
    stack.attach();
    region.attach();

    std::vector<cw::Offset> hovers;
    int enters = 0;
    region.on_enter = [&] { ++enters; };
    region.on_hover = [&](cw::Offset p) { hovers.push_back(p); };

    cw::PointerEvent move;
    move.kind        = cw::PointerEventKind::move;
    move.device_kind = cw::PointerDeviceKind::mouse;
    move.pressure    = 0.0f;
    move.position    = {60.0f, 40.0f};
    dispatcher->handlePointerEvent(move);
    move.position = {149.0f, 89.0f};
    dispatcher->handlePointerEvent(move);

    EXPECT_EQ(enters, 1);
    ASSERT_EQ(hovers.size(), 2u);
    EXPECT_FLOAT_EQ(hovers[0].x, 10.0f);
    EXPECT_FLOAT_EQ(hovers[0].y, 10.0f);
    EXPECT_FLOAT_EQ(hovers[1].x, 99.0f);
    EXPECT_FLOAT_EQ(hovers[1].y, 59.0f);

    region.detach();
    stack.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
