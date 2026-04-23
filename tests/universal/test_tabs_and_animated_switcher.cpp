#include <gtest/gtest.h>
#include <campello_widgets/widgets/tab_bar.hpp>
#include <campello_widgets/widgets/animated_switcher.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/render_opacity.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Probe widget — tracks lifecycle counts
// ---------------------------------------------------------------------------

class ProbeWidget : public cw::StatefulWidget
{
public:
    mutable int init_count    = 0;
    mutable int dispose_count = 0;
    mutable int build_count   = 0;

    std::unique_ptr<cw::StateBase> createState() const override;
};

class ProbeWidgetState : public cw::State<ProbeWidget>
{
public:
    void initState() override
    {
        ++widget().init_count;
    }

    void dispose() override
    {
        ++widget().dispose_count;
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        ++widget().build_count;
        return std::make_shared<cw::SizedBox>(100.0f, 100.0f);
    }
};

std::unique_ptr<cw::StateBase> ProbeWidget::createState() const
{
    return std::make_unique<ProbeWidgetState>();
}

// ---------------------------------------------------------------------------
// Helper to create a simple button (GestureDetector + Container)
// ---------------------------------------------------------------------------

static cw::WidgetRef makeButton(bool& tapped_flag)
{
    auto container = std::make_shared<cw::Container>();
    container->width  = 80.0f;
    container->height = 40.0f;

    auto det = std::make_shared<cw::GestureDetector>();
    det->on_tap = [&tapped_flag]() { tapped_flag = true; };
    det->child  = container;
    return det;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class TabWidgetsTest : public ::testing::Test
{
protected:
    std::unique_ptr<cw::TickerScheduler>   ticker_;
    std::unique_ptr<cw::PointerDispatcher> dispatcher_;

    void SetUp() override
    {
        ticker_ = std::make_unique<cw::TickerScheduler>();
        cw::TickerScheduler::setActive(ticker_.get());

        dispatcher_ = std::make_unique<cw::PointerDispatcher>();
        cw::PointerDispatcher::setActiveDispatcher(dispatcher_.get());
    }

    void TearDown() override
    {
        cw::PointerDispatcher::setActiveDispatcher(nullptr);
        cw::TickerScheduler::setActive(nullptr);
    }

    // Mount a widget tree, layout the render tree, and return the root RenderBox.
    std::shared_ptr<cw::RenderBox> mountAndLayout(cw::WidgetRef root_widget,
                                                   float        w,
                                                   float        h)
    {
        root_element_ = root_widget->createElement();
        root_element_->mount(nullptr);

        auto* roe = root_element_->findDescendantRenderObjectElement();
        EXPECT_NE(roe, nullptr);

        auto box = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
        EXPECT_NE(box, nullptr);

        box->layout(cw::BoxConstraints::tight(w, h));
        return box;
    }

    // Advance the ticker so animations complete.
    void pumpAnimation(double duration_ms = 300.0)
    {
        // AnimationController::onTick ignores the first tick (sets baseline).
        // So we need two ticks with a delta >= duration_ms.
        ticker_->tick(1000);
        ticker_->tick(1000 + static_cast<uint64_t>(duration_ms + 50));
    }

    // Hit-test the root render box at a local position.
    cw::HitTestResult hitTest(const std::shared_ptr<cw::RenderBox>& root,
                              const cw::Offset&                     pos)
    {
        cw::HitTestResult result;
        root->hitTest(result, pos);
        return result;
    }

    // Find the first RenderGestureDetector in a hit path.
    static cw::RenderGestureDetector* findGestureDetector(const cw::HitTestResult& result)
    {
        for (const auto& entry : result.path())
        {
            if (auto* det = dynamic_cast<cw::RenderGestureDetector*>(entry.target))
                return det;
        }
        return nullptr;
    }

    // Find the first RenderStack in a hit path.
    static cw::RenderStack* findRenderStack(const cw::HitTestResult& result)
    {
        for (const auto& entry : result.path())
        {
            if (auto* stack = dynamic_cast<cw::RenderStack*>(entry.target))
                return stack;
        }
        return nullptr;
    }

    // Find the first RenderOpacity in a hit path.
    static cw::RenderOpacity* findRenderOpacity(const cw::HitTestResult& result)
    {
        for (const auto& entry : result.path())
        {
            if (auto* op = dynamic_cast<cw::RenderOpacity*>(entry.target))
                return op;
        }
        return nullptr;
    }

private:
    std::shared_ptr<cw::Element> root_element_;
};

// ---------------------------------------------------------------------------
// DefaultTabController
// ---------------------------------------------------------------------------

TEST_F(TabWidgetsTest, DefaultTabController_InitialIndexPropagated)
{
    auto probe0 = std::make_shared<ProbeWidget>();
    auto probe1 = std::make_shared<ProbeWidget>();
    auto probe2 = std::make_shared<ProbeWidget>();

    auto tabView = std::make_shared<cw::TabBarView>();
    tabView->children = {probe0, probe1, probe2};

    auto controller = std::make_shared<cw::DefaultTabController>(3, 1);
    controller->child = tabView;

    mountAndLayout(controller, 400.0f, 400.0f);

    // initial_index = 1, so probe1 should have been built.
    EXPECT_EQ(probe0->init_count, 0);
    EXPECT_EQ(probe1->init_count, 1);
    EXPECT_EQ(probe2->init_count, 0);
}

// ---------------------------------------------------------------------------
// TabBar
// ---------------------------------------------------------------------------

TEST_F(TabWidgetsTest, TabBar_BuildsCorrectNumberOfTabs)
{
    auto tabView = std::make_shared<cw::TabBarView>();
    tabView->children = {
        std::make_shared<cw::SizedBox>(100.0f, 100.0f),
        std::make_shared<cw::SizedBox>(100.0f, 100.0f),
        std::make_shared<cw::SizedBox>(100.0f, 100.0f),
    };

    auto bar = std::make_shared<cw::TabBar>();
    bar->tabs = {cw::Tab{"A"}, cw::Tab{"B"}, cw::Tab{"C"}};

    auto col = std::make_shared<cw::Column>();
    col->children = {bar, std::make_shared<cw::Expanded>(tabView)};

    auto controller = std::make_shared<cw::DefaultTabController>();
    controller->length = 3;
    controller->child  = col;

    auto root = mountAndLayout(controller, 300.0f, 400.0f);

    // Hit-test in the middle of each tab (tab bar is at the top, ~40px tall).
    // Tab 0 centre ≈ (50, 20)
    auto r0 = hitTest(root, {50.0f, 20.0f});
    EXPECT_NE(findGestureDetector(r0), nullptr);

    // Tab 1 centre ≈ (150, 20)
    auto r1 = hitTest(root, {150.0f, 20.0f});
    EXPECT_NE(findGestureDetector(r1), nullptr);

    // Tab 2 centre ≈ (250, 20)
    auto r2 = hitTest(root, {250.0f, 20.0f});
    EXPECT_NE(findGestureDetector(r2), nullptr);
}

TEST_F(TabWidgetsTest, TabBar_TapChangesTab)
{
    auto probe0 = std::make_shared<ProbeWidget>();
    auto probe1 = std::make_shared<ProbeWidget>();

    auto tabView = std::make_shared<cw::TabBarView>();
    tabView->children = {probe0, probe1};

    auto bar = std::make_shared<cw::TabBar>();
    bar->tabs = {cw::Tab{"Tab0"}, cw::Tab{"Tab1"}};

    auto col = std::make_shared<cw::Column>();
    col->children = {bar, std::make_shared<cw::Expanded>(tabView)};

    auto controller = std::make_shared<cw::DefaultTabController>();
    controller->length = 2;
    controller->child  = col;

    auto root = mountAndLayout(controller, 300.0f, 400.0f);

    // Initially probe0 is active.
    EXPECT_EQ(probe0->init_count, 1);
    EXPECT_EQ(probe1->init_count, 0);

    // Hit-test tab 1 and simulate a tap.
    auto r1 = hitTest(root, {200.0f, 20.0f});
    auto* det1 = findGestureDetector(r1);
    ASSERT_NE(det1, nullptr);
    ASSERT_TRUE(det1->on_tap);
    det1->on_tap();

    pumpAnimation(300.0);

    // probe1 is initialised once during the transition (inside the Stack)
    // and once after the transition when the Stack is unmounted.
    EXPECT_EQ(probe1->init_count, 2);
}

// ---------------------------------------------------------------------------
// TabBarView
// ---------------------------------------------------------------------------

TEST_F(TabWidgetsTest, TabBarView_ClampsIndexToValidRange)
{
    auto probe0 = std::make_shared<ProbeWidget>();

    auto tabView = std::make_shared<cw::TabBarView>();
    tabView->children = {probe0};

    // Build with an out-of-range index (controller length > children size).
    auto controller = std::make_shared<cw::DefaultTabController>();
    controller->length = 5;        // claims 5 tabs
    controller->child  = tabView;  // but only 1 child

    mountAndLayout(controller, 400.0f, 400.0f);

    // Should fall back to index 0 and build probe0.
    EXPECT_EQ(probe0->init_count, 1);
}

// ---------------------------------------------------------------------------
// AnimatedSwitcher — isolated behaviour
// ---------------------------------------------------------------------------

TEST_F(TabWidgetsTest, AnimatedSwitcher_ShowsSingleChildWhenIdle)
{
    auto child = std::make_shared<cw::SizedBox>(100.0f, 100.0f);

    auto switcher = std::make_shared<cw::AnimatedSwitcher>();
    switcher->child      = child;
    switcher->duration_ms = 200.0;

    auto root = mountAndLayout(switcher, 400.0f, 400.0f);

    // Hit-test anywhere inside the box.
    auto result = hitTest(root, {50.0f, 50.0f});

    // Should NOT find a RenderStack — the child is returned directly.
    EXPECT_EQ(findRenderStack(result), nullptr);
}

TEST_F(TabWidgetsTest, AnimatedSwitcher_TransitionCreatesStack)
{
    auto childA = std::make_shared<cw::SizedBox>(100.0f, 100.0f);
    auto childB = std::make_shared<cw::SizedBox>(100.0f, 100.0f);

    auto switcher = std::make_shared<cw::AnimatedSwitcher>();
    switcher->child       = childA;
    switcher->duration_ms = 200.0;

    auto element = switcher->createElement();
    element->mount(nullptr);
    auto* roe = element->findDescendantRenderObjectElement();
    ASSERT_NE(roe, nullptr);
    auto root = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
    root->layout(cw::BoxConstraints::tight(400.0f, 400.0f));

    // Update with a different child to start a transition.
    auto switcher2 = std::make_shared<cw::AnimatedSwitcher>();
    switcher2->child       = childB;
    switcher2->duration_ms = 200.0;
    element->update(switcher2);

    // Re-layout after the rebuild triggered by update().
    roe = element->findDescendantRenderObjectElement();
    root = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
    root->layout(cw::BoxConstraints::tight(400.0f, 400.0f));

    // During transition the render tree should contain a RenderStack.
    auto result = hitTest(root, {50.0f, 50.0f});
    EXPECT_NE(findRenderStack(result), nullptr);
}

TEST_F(TabWidgetsTest, AnimatedSwitcher_TransitionCompletes)
{
    auto probeA = std::make_shared<ProbeWidget>();
    auto probeB = std::make_shared<ProbeWidget>();

    auto switcher = std::make_shared<cw::AnimatedSwitcher>();
    switcher->child       = probeA;
    switcher->duration_ms = 200.0;

    auto element = switcher->createElement();
    element->mount(nullptr);

    // Update to childB.
    auto switcher2 = std::make_shared<cw::AnimatedSwitcher>();
    switcher2->child       = probeB;
    switcher2->duration_ms = 200.0;
    element->update(switcher2);

    // Pump frames until the transition is done.
    pumpAnimation(250.0);

    // After completion the render tree should be back to a single child.
    auto* roe = element->findDescendantRenderObjectElement();
    ASSERT_NE(roe, nullptr);
    auto root = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
    root->layout(cw::BoxConstraints::tight(400.0f, 400.0f));

    auto result = hitTest(root, {50.0f, 50.0f});
    EXPECT_EQ(findRenderStack(result), nullptr);

    // probeA is disposed once when switching away and once when the Stack
    // is unmounted at the end of the transition.
    EXPECT_EQ(probeA->dispose_count, 2);
    // probeB is disposed once when the Stack is unmounted, then recreated.
    EXPECT_EQ(probeB->dispose_count, 1);
}

TEST_F(TabWidgetsTest, AnimatedSwitcher_RapidSwitch)
{
    auto probeA = std::make_shared<ProbeWidget>();
    auto probeB = std::make_shared<ProbeWidget>();

    auto switcher = std::make_shared<cw::AnimatedSwitcher>();
    switcher->child       = probeA;
    switcher->duration_ms = 200.0;

    auto element = switcher->createElement();
    element->mount(nullptr);

    // A -> B (start transition, don't let it finish)
    auto s2 = std::make_shared<cw::AnimatedSwitcher>();
    s2->child       = probeB;
    s2->duration_ms = 200.0;
    element->update(s2);

    // B -> A (immediately switch back)
    auto s3 = std::make_shared<cw::AnimatedSwitcher>();
    s3->child       = probeA;
    s3->duration_ms = 200.0;
    element->update(s3);

    pumpAnimation(250.0);

    // Should settle without crashing.
    auto* roe = element->findDescendantRenderObjectElement();
    ASSERT_NE(roe, nullptr);
}

// ---------------------------------------------------------------------------
// Integration: full tab switch + hit testing
// ---------------------------------------------------------------------------

TEST_F(TabWidgetsTest, Integration_SwitchTabsAndHitTest)
{
    bool btn0_tapped = false;
    bool btn1_tapped = false;

    // Tab 0: a button at (50, 150)
    auto btn0 = makeButton(btn0_tapped);
    auto page0 = std::make_shared<cw::Align>();
    page0->alignment = cw::Alignment::topLeft();
    page0->child     = btn0;

    // Tab 1: a button at (50, 150)
    auto btn1 = makeButton(btn1_tapped);
    auto page1 = std::make_shared<cw::Align>();
    page1->alignment = cw::Alignment::topLeft();
    page1->child     = btn1;

    auto tabView = std::make_shared<cw::TabBarView>();
    tabView->children = {page0, page1};

    auto bar = std::make_shared<cw::TabBar>();
    bar->tabs = {cw::Tab{"Tab0"}, cw::Tab{"Tab1"}};

    auto col = std::make_shared<cw::Column>();
    col->children = {bar, std::make_shared<cw::Expanded>(tabView)};

    auto controller = std::make_shared<cw::DefaultTabController>();
    controller->length = 2;
    controller->child  = col;

    auto root = mountAndLayout(controller, 300.0f, 400.0f);

    // ---- Verify tab 0 button is hittable ----
    // The button is 80x40, aligned top-left inside the tab content area
    // which starts just below the tab bar (~24px tall).
    {
        auto r = hitTest(root, {40.0f, 44.0f});
        auto* det = findGestureDetector(r);
        ASSERT_NE(det, nullptr);
        ASSERT_TRUE(det->on_tap);
        det->on_tap();
        EXPECT_TRUE(btn0_tapped);
    }

    // ---- Switch to tab 1 ----
    // Tap the second tab in the tab bar (centre roughly at x=225, y=12).
    {
        auto r = hitTest(root, {225.0f, 12.0f});
        auto* det = findGestureDetector(r);
        ASSERT_NE(det, nullptr);
        det->on_tap();
        pumpAnimation(300.0);
    }

    // Verify tab 1 button works.
    {
        root->layout(cw::BoxConstraints::tight(300.0f, 400.0f));
        auto r = hitTest(root, {40.0f, 44.0f});
        auto* det = findGestureDetector(r);
        ASSERT_NE(det, nullptr);
        det->on_tap();
        EXPECT_TRUE(btn1_tapped);
    }

    // ---- Switch back to tab 0 ----
    {
        auto r = hitTest(root, {75.0f, 12.0f});
        auto* det = findGestureDetector(r);
        ASSERT_NE(det, nullptr);
        det->on_tap();
        pumpAnimation(300.0);
    }

    // ---- CRITICAL: verify tab 0 button still responds after coming back ----
    {
        root->layout(cw::BoxConstraints::tight(300.0f, 400.0f));
        auto r = hitTest(root, {40.0f, 44.0f});
        auto* det = findGestureDetector(r);
        ASSERT_NE(det, nullptr)
            << "Hit-test did not find a GestureDetector after switching back to tab 0. "
               "This reproduces the bug where the initial tab stops responding to clicks.";

        // Reset flag and tap again.
        btn0_tapped = false;
        ASSERT_TRUE(det->on_tap);
        det->on_tap();
        EXPECT_TRUE(btn0_tapped)
            << "Button on initial tab did not receive tap after switching back.";
    }
}
