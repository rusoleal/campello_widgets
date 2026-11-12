// WindowController / Window widget (Phase 17, M2) against a fake
// PlatformWindowing, so the core semantics are tested without a GPU or
// native windows:
//   - the hosted subtree is a *separate* render tree rooted in the other
//     window (the parent window only sees a zero-size placeholder),
//   - its elements, render objects, focus and frame requests belong to the
//     other window's View,
//   - MediaQuery describes the hosted window and follows its metrics,
//   - destroy() unmounts the subtree while the window still exists,
//   - close requests, and removing the Window widget from the tree.

#include <gtest/gtest.h>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/paint_context.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // ------------------------------------------------------------------
    // Fake platform
    // ------------------------------------------------------------------

    struct WinTestLog
    {
        int  windows_alive = 0;
        int  shows         = 0;
        bool last_window_destroyed = false;
    };

    class WinTestFakeWindow : public cw::PlatformWindow
    {
    public:
        explicit WinTestFakeWindow(WinTestLog& log, const cw::WindowOptions& options)
            : log_(log)
        {
            ++log_.windows_alive;
            metrics_.logical_size       = options.size;
            metrics_.device_pixel_ratio = 2.0f;
        }
        ~WinTestFakeWindow() override
        {
            --log_.windows_alive;
            log_.last_window_destroyed = true;
        }

        cw::View& view() override { return view_; }
        void setRootRenderBox(std::shared_ptr<cw::RenderBox> root) override { root_ = std::move(root); ++root_sets; }
        void setTitle(const std::string& title) override { title_ = title; }
        void setSize(cw::Size s) override { metrics_.logical_size = s; }
        cw::WindowMetrics metrics() const override { return metrics_; }
        void show() override { ++log_.shows; }
        void requestClose() override { auto cb = on_close_requested; if (cb) cb(); }

        // Test controls
        void changeMetrics(cw::WindowMetrics m)
        {
            metrics_ = m;
            auto cb = on_metrics_changed;
            if (cb) cb();
        }

        std::shared_ptr<cw::RenderBox> root_;
        int                            root_sets = 0;
        std::string                    title_;

    private:
        WinTestLog&       log_;
        cw::View          view_;
        cw::WindowMetrics metrics_;
    };

    class WinTestFakeWindowing : public cw::PlatformWindowing
    {
    public:
        WinTestLog          log;
        WinTestFakeWindow*  last = nullptr;
        std::unique_ptr<cw::PlatformWindow> createWindow(const cw::WindowOptions& options) override
        {
            auto w = std::make_unique<WinTestFakeWindow>(log, options);
            last = w.get();
            return w;
        }
    };

    // Installs the fake for one test.
    struct WinTestPlatform
    {
        WinTestFakeWindowing windowing;
        WinTestPlatform()  { cw::PlatformWindowing::setInstance(&windowing); }
        ~WinTestPlatform() { cw::PlatformWindowing::setInstance(nullptr); }
    };

    // ------------------------------------------------------------------
    // Widgets
    // ------------------------------------------------------------------

    // Records the MediaQuery it sees and the element it was built in.
    class WinTestProbe : public cw::StatelessWidget
    {
    public:
        mutable cw::Size     seen_size{-1.0f, -1.0f};
        mutable float        seen_dpr = -1.0f;
        mutable cw::Element* element  = nullptr;

        cw::WidgetRef build(cw::BuildContext& context) const override
        {
            if (const auto* mq = cw::MediaQuery::of(context))
            {
                seen_size = mq->logical_size;
                seen_dpr  = mq->device_pixel_ratio;
            }
            element = dynamic_cast<cw::Element*>(&context);
            return std::make_shared<cw::SizedBox>(30.0f, 20.0f);
        }
    };

    // Tracks dispose() relative to the fake window's lifetime.
    class WinTestDisposeProbe : public cw::StatefulWidget
    {
    public:
        WinTestLog* log             = nullptr;
        bool*       window_was_alive_at_dispose = nullptr;
        std::unique_ptr<cw::StateBase> createState() const override;
    };
    class WinTestDisposeProbeState : public cw::State<WinTestDisposeProbe>
    {
    public:
        void dispose() override
        {
            *widget().window_was_alive_at_dispose = widget().log->windows_alive > 0;
        }
        cw::WidgetRef build(cw::BuildContext&) override
        {
            return std::make_shared<cw::SizedBox>(1.0f, 1.0f);
        }
    };
    std::unique_ptr<cw::StateBase> WinTestDisposeProbe::createState() const
    {
        return std::make_unique<WinTestDisposeProbeState>();
    }

    // Main tree: Padding(child: either Window(controller, hosted) or a SizedBox).
    class WinTestHost : public cw::StatefulWidget
    {
    public:
        std::shared_ptr<cw::WindowController> controller;
        cw::WidgetRef                         hosted;
        bool*                                 show_window = nullptr;
        std::unique_ptr<cw::StateBase> createState() const override;
    };
    class WinTestHostState : public cw::State<WinTestHost>
    {
    public:
        cw::WidgetRef build(cw::BuildContext&) override
        {
            cw::WidgetRef inner = *widget().show_window
                ? cw::WidgetRef(std::make_shared<cw::Window>(widget().controller, widget().hosted))
                : cw::WidgetRef(std::make_shared<cw::SizedBox>(5.0f, 5.0f));
            return std::make_shared<cw::Padding>(cw::EdgeInsets::all(1.0f), inner);
        }
    };
    std::unique_ptr<cw::StateBase> WinTestHost::createState() const
    {
        return std::make_unique<WinTestHostState>();
    }

    // Mounts `host` as the main window's tree (in `main_view`).
    std::shared_ptr<cw::Element> winTestMount(cw::View& main_view, cw::WidgetRef host)
    {
        cw::View::Scope scope(&main_view);
        auto root = std::make_shared<cw::MediaQuery>(cw::MediaQueryData{}, host)->createElement();
        root->mount(nullptr);
        return root;
    }

    cw::RenderPadding* winTestMainPadding(cw::Element* root)
    {
        auto* roe = root->findDescendantRenderObjectElement();
        return roe ? dynamic_cast<cw::RenderPadding*>(roe->renderObject()) : nullptr;
    }
}

// ---------------------------------------------------------------------------

TEST(Window, CreateWithoutPlatformWindowingReturnsNull)
{
    cw::PlatformWindowing::setInstance(nullptr);
    EXPECT_EQ(cw::WindowController::create({}), nullptr);
}

TEST(Window, CreateShowsTheWindow)
{
    WinTestPlatform platform;
    auto controller = cw::WindowController::create({.title = "Inspector", .size = {300.0f, 200.0f}});
    ASSERT_NE(controller, nullptr);
    EXPECT_TRUE(controller->isOpen());
    EXPECT_EQ(platform.windowing.log.shows, 1);
    EXPECT_FLOAT_EQ(controller->metrics().logical_size.width, 300.0f);
}

TEST(Window, HostedChildIsTheRootOfTheOtherWindowsRenderTree)
{
    WinTestPlatform platform;
    cw::View main_view;
    auto controller = cw::WindowController::create({.size = {300.0f, 200.0f}});
    auto* fake = platform.windowing.last;

    bool show = true;
    auto host = std::make_shared<WinTestHost>();
    host->controller  = controller;
    host->hosted      = std::make_shared<cw::SizedBox>(30.0f, 20.0f);
    host->show_window = &show;
    auto root = winTestMount(main_view, host);

    // The other window got a root, with no render parent, owned by its View.
    ASSERT_NE(fake->root_, nullptr);
    EXPECT_EQ(fake->root_->parent(), nullptr);
    EXPECT_EQ(fake->root_->view(), &fake->view());

    // The main window only holds the zero-size placeholder.
    auto* padding = winTestMainPadding(root.get());
    ASSERT_NE(padding, nullptr);
    padding->layout(cw::BoxConstraints::loose(100.0f, 100.0f));
    EXPECT_FLOAT_EQ(padding->size().width, 2.0f);   // 0 + 1 + 1 insets
    EXPECT_FLOAT_EQ(padding->size().height, 2.0f);
    EXPECT_EQ(padding->view(), &main_view);

    root->unmount();
}

TEST(Window, HostedElementsBelongToTheWindowsViewAndSeeItsMetrics)
{
    WinTestPlatform platform;
    cw::View main_view;
    auto controller = cw::WindowController::create({.size = {300.0f, 200.0f}});
    auto* fake = platform.windowing.last;

    bool show = true;
    auto probe = std::make_shared<WinTestProbe>();
    auto host = std::make_shared<WinTestHost>();
    host->controller = controller; host->hosted = probe; host->show_window = &show;
    auto root = winTestMount(main_view, host);

    ASSERT_NE(probe->element, nullptr);
    EXPECT_EQ(probe->element->view(), &fake->view());
    EXPECT_FLOAT_EQ(probe->seen_size.width, 300.0f);
    EXPECT_FLOAT_EQ(probe->seen_dpr, 2.0f);

    // The window is resized: the hosted subtree gets a new MediaQuery.
    cw::WindowMetrics m;
    m.logical_size = {640.0f, 480.0f};
    m.device_pixel_ratio = 1.0f;
    fake->changeMetrics(m);
    cw::Element::buildScope();
    EXPECT_FLOAT_EQ(probe->seen_size.width, 640.0f);
    EXPECT_FLOAT_EQ(probe->seen_dpr, 1.0f);

    root->unmount();
}

TEST(Window, InvalidationInTheHostedTreeRequestsAFrameFromThatWindowOnly)
{
    WinTestPlatform platform;
    cw::View main_view;
    auto controller = cw::WindowController::create({.size = {100.0f, 100.0f}});
    auto* fake = platform.windowing.last;
    int main_frames = 0, window_frames = 0;
    main_view.setFrameRequestCallback([&] { ++main_frames; });
    fake->view().setFrameRequestCallback([&] { ++window_frames; });

    bool show = true;
    auto host = std::make_shared<WinTestHost>();
    host->controller = controller;
    host->hosted = std::make_shared<cw::SizedBox>(30.0f, 20.0f);
    host->show_window = &show;
    auto root = winTestMount(main_view, host);
    ASSERT_NE(fake->root_, nullptr);

    fake->root_->layout(cw::BoxConstraints::loose(100.0f, 100.0f));
    cw::PaintContext ctx(100.0f, 100.0f);
    fake->root_->paint(ctx, cw::Offset::zero());
    main_frames = window_frames = 0;

    fake->root_->markNeedsPaint();
    EXPECT_EQ(window_frames, 1);
    EXPECT_EQ(main_frames, 0);
    root->unmount();
}

TEST(Window, FocusInTheHostedTreeBelongsToThatWindow)
{
    WinTestPlatform platform;
    cw::View main_view;
    main_view.makePrimary();
    auto controller = cw::WindowController::create({.size = {100.0f, 100.0f}});
    auto* fake = platform.windowing.last;

    int keys = 0;
    auto node = std::make_shared<cw::FocusNode>();
    node->on_key = [&](const cw::KeyEvent&) { ++keys; return true; };
    auto focus = std::make_shared<cw::Focus>();
    focus->focus_node = node;
    focus->auto_focus = true;
    focus->child = std::make_shared<cw::SizedBox>(1.0f, 1.0f);

    bool show = true;
    auto host = std::make_shared<WinTestHost>();
    host->controller = controller; host->hosted = focus; host->show_window = &show;
    auto root = winTestMount(main_view, host);

    cw::KeyEvent key;
    main_view.focusManager().handleKeyEvent(key);
    EXPECT_EQ(keys, 0) << "focus leaked into the main window";
    fake->view().focusManager().handleKeyEvent(key);
    EXPECT_EQ(keys, 1);
    root->unmount();
}

TEST(Window, DestroyUnmountsTheHostedTreeBeforeTheWindowGoes)
{
    WinTestPlatform platform;
    cw::View main_view;
    auto controller = cw::WindowController::create({});
    bool destroyed_cb = false;
    controller->on_destroyed = [&] { destroyed_cb = true; };

    bool alive_at_dispose = false;
    auto probe = std::make_shared<WinTestDisposeProbe>();
    probe->log = &platform.windowing.log;
    probe->window_was_alive_at_dispose = &alive_at_dispose;

    bool show = true;
    auto host = std::make_shared<WinTestHost>();
    host->controller = controller; host->hosted = probe; host->show_window = &show;
    auto root = winTestMount(main_view, host);
    ASSERT_EQ(platform.windowing.log.windows_alive, 1);

    controller->destroy();
    EXPECT_TRUE(alive_at_dispose) << "hosted State disposed after its window was destroyed";
    EXPECT_EQ(platform.windowing.log.windows_alive, 0);
    EXPECT_FALSE(controller->isOpen());
    EXPECT_TRUE(destroyed_cb);

    // The Window widget is still in the tree: it now renders nothing, and
    // rebuilding it is harmless.
    root->markNeedsBuild();
    cw::Element::buildScope();
    root->unmount();
}

TEST(Window, CloseRequestDestroysByDefaultUnlessHandled)
{
    WinTestPlatform platform;
    auto a = cw::WindowController::create({});
    a->requestClose();
    EXPECT_FALSE(a->isOpen());

    auto b = cw::WindowController::create({});
    int asked = 0;
    b->on_close_requested = [&](cw::WindowController&) { ++asked; };  // e.g. "unsaved changes?"
    b->requestClose();
    EXPECT_EQ(asked, 1);
    EXPECT_TRUE(b->isOpen());
}

TEST(Window, RemovingTheWindowWidgetUnmountsTheHostedTree)
{
    WinTestPlatform platform;
    cw::View main_view;
    auto controller = cw::WindowController::create({});
    auto* fake = platform.windowing.last;

    bool alive_at_dispose = false;
    bool disposed_marker  = false;
    auto probe = std::make_shared<WinTestDisposeProbe>();
    probe->log = &platform.windowing.log;
    probe->window_was_alive_at_dispose = &disposed_marker;

    bool show = true;
    auto host = std::make_shared<WinTestHost>();
    host->controller = controller; host->hosted = probe; host->show_window = &show;
    auto root = winTestMount(main_view, host);
    ASSERT_NE(fake->root_, nullptr);

    show = false;  // the app stops showing the window's content
    root->firstChildElement()->markNeedsBuild();  // the WinTestHost element
    cw::Element::buildScope();
    EXPECT_TRUE(disposed_marker);        // hosted State disposed (window alive)
    EXPECT_EQ(fake->root_, nullptr);     // window's render root cleared
    EXPECT_TRUE(controller->isOpen());   // the window itself stays open
    (void)alive_at_dispose;
    root->unmount();
}

namespace
{
    // A platform with a real "next UI turn": posted tasks wait in a queue.
    class WinTestQueuedWindowing : public WinTestFakeWindowing
    {
    public:
        std::vector<std::function<void()>> tasks;
        void postTask(std::function<void()> task) override { tasks.push_back(std::move(task)); }
        void runPostedTasks()
        {
            auto pending = std::move(tasks);
            tasks.clear();
            for (auto& t : pending) t();
        }
    };
}

TEST(Window, DestroyIsDeferredToTheNextUiTurn)
{
    // A "Close" button calls destroy() from inside the window's own event
    // dispatch: the window (its dispatcher, the button's render object) must
    // survive until that dispatch has unwound.
    WinTestQueuedWindowing windowing;
    cw::PlatformWindowing::setInstance(&windowing);
    {
        auto controller = cw::WindowController::create({});
        bool destroyed = false;
        controller->on_destroyed = [&] { destroyed = true; };

        controller->destroy();
        EXPECT_TRUE(controller->isOpen()) << "torn down synchronously";
        EXPECT_FALSE(destroyed);
        EXPECT_EQ(windowing.log.windows_alive, 1);

        controller->destroy();  // a second request while pending is a no-op
        EXPECT_EQ(windowing.tasks.size(), 1u);

        windowing.runPostedTasks();
        EXPECT_FALSE(controller->isOpen());
        EXPECT_TRUE(destroyed);
        EXPECT_EQ(windowing.log.windows_alive, 0);
    }
    cw::PlatformWindowing::setInstance(nullptr);
}

namespace
{
    // A window of the given kind, as a platform layer would open it by itself.
    class WinTestKindWindow : public WinTestFakeWindow
    {
    public:
        WinTestKindWindow(WinTestLog& log, cw::WindowKind kind)
            : WinTestFakeWindow(log, cw::WindowOptions{}), kind_(kind) {}
        cw::WindowKind kind() const override { return kind_; }
    private:
        cw::WindowKind kind_;
    };
}

TEST(Window, PlatformOpenedWindowsGoToTheHandler)
{
    // iPadOS: the user asks for another window, or a display connects.
    WinTestLog log;
    cw::WindowController::setPlatformWindowHandler(nullptr);

    // No handler: the platform keeps the window (and closes it).
    std::unique_ptr<cw::PlatformWindow> window =
        std::make_unique<WinTestKindWindow>(log, cw::WindowKind::external_display);
    EXPECT_FALSE(cw::WindowController::offerPlatformWindow(window));
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(log.windows_alive, 1);

    // With a handler: it gets an open controller of that kind.
    std::shared_ptr<cw::WindowController> adopted;
    cw::WindowController::setPlatformWindowHandler([&](std::shared_ptr<cw::WindowController> c) {
        adopted = std::move(c);
    });
    EXPECT_TRUE(cw::WindowController::offerPlatformWindow(window));
    EXPECT_EQ(window, nullptr);
    ASSERT_NE(adopted, nullptr);
    EXPECT_TRUE(adopted->isOpen());
    EXPECT_EQ(adopted->kind(), cw::WindowKind::external_display);

    // Its close requests work as for created windows (default: destroy).
    adopted->requestClose();
    EXPECT_FALSE(adopted->isOpen());
    EXPECT_EQ(log.windows_alive, 0);

    cw::WindowController::setPlatformWindowHandler(nullptr);
}

TEST(Window, KindDefaultsToRegular)
{
    WinTestPlatform platform;
    auto controller = cw::WindowController::create({.kind = cw::WindowKind::dialog});
    ASSERT_NE(controller, nullptr);
    // The fake doesn't override kind(): regular, like a platform without dialogs.
    EXPECT_EQ(controller->kind(), cw::WindowKind::regular);
}
