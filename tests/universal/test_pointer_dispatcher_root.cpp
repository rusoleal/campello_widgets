#include <gtest/gtest.h>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// A tree's root render object has no parent, and attach() otherwise runs only
// when a render object gets one (RenderObject::setParent()): setRoot()
// attaches a parentless root and detaches the root it replaces. Before, a
// GestureDetector that was a tree's root render object never registered for
// input on any platform.

namespace
{
    struct CountingBox : cw::RenderBox
    {
        int attaches = 0;
        int detaches = 0;
        void attach() override { ++attaches; }
        void detach() override { ++detaches; }
    };

    void tap(cw::PointerDispatcher& dispatcher, float x, float y)
    {
        for (auto kind : {cw::PointerEventKind::down, cw::PointerEventKind::up})
        {
            cw::PointerEvent e;
            e.kind     = kind;
            e.position = {x, y};
            dispatcher.handlePointerEvent(e);
        }
    }
} // namespace

TEST(PointerDispatcherRoot, SetRootAttachesAParentlessRoot)
{
    cw::PointerDispatcher dispatcher;
    auto root = std::make_shared<CountingBox>();
    dispatcher.setRoot(root);
    EXPECT_EQ(root->attaches, 1);
    EXPECT_EQ(root->detaches, 0);
}

TEST(PointerDispatcherRoot, SettingTheSameRootAgainDoesNotReattach)
{
    cw::PointerDispatcher dispatcher;
    auto root = std::make_shared<CountingBox>();
    dispatcher.setRoot(root);
    dispatcher.setRoot(root);
    EXPECT_EQ(root->attaches, 1);
    EXPECT_EQ(root->detaches, 0);
}

TEST(PointerDispatcherRoot, ReplacingOrClearingTheRootDetachesTheOldOne)
{
    cw::PointerDispatcher dispatcher;
    auto first  = std::make_shared<CountingBox>();
    auto second = std::make_shared<CountingBox>();
    dispatcher.setRoot(first);
    dispatcher.setRoot(second);
    EXPECT_EQ(first->detaches, 1);
    EXPECT_EQ(second->attaches, 1);

    dispatcher.setRoot(nullptr);
    EXPECT_EQ(second->detaches, 1);
}

TEST(PointerDispatcherRoot, ARootWithAParentIsLeftToItsParent)
{
    // Already attached through its parent: not attached (or detached) again.
    cw::PointerDispatcher dispatcher;
    auto parent = std::make_shared<cw::RenderBox>();
    auto child  = std::make_shared<CountingBox>();
    parent->setChild(child);
    ASSERT_EQ(child->attaches, 1);

    dispatcher.setRoot(child);
    dispatcher.setRoot(nullptr);
    EXPECT_EQ(child->attaches, 1);
    EXPECT_EQ(child->detaches, 0);
}

TEST(PointerDispatcherRoot, AGestureDetectorAtTheRootGetsTaps)
{
    auto dispatcher = std::make_shared<cw::PointerDispatcher>();
    cw::PointerDispatcher::setActiveDispatcher(dispatcher.get());

    auto detector = std::make_shared<cw::RenderGestureDetector>();
    int taps = 0;
    detector->on_tap = [&taps] { ++taps; };
    detector->layout(cw::BoxConstraints::tight(100.0f, 100.0f));

    dispatcher->setRoot(detector);  // no manual attach()
    tap(*dispatcher, 50.0f, 50.0f);
    EXPECT_EQ(taps, 1);

    dispatcher->setRoot(nullptr);

    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
