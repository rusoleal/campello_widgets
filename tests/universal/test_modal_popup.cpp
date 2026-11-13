// Modal windows, owned windows, per-window overlays and PopupWindowAnchor
// (Phase 17 M2 follow-ups + M7), against a fake PlatformWindowing.

#include <gtest/gtest.h>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/popup_window_anchor.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/paint_context.hpp>

#include <functional>
#include <vector>

namespace cw = systems::leal::campello_widgets;

namespace
{
    class MpWindow : public cw::PlatformWindow
    {
    public:
        explicit MpWindow(const cw::WindowOptions& o) : options(o) { metrics_.logical_size = o.size; }
        cw::View& view() override { return view_; }
        void setRootRenderBox(std::shared_ptr<cw::RenderBox> root) override { root_ = std::move(root); }
        void setTitle(const std::string&) override {}
        void setSize(cw::Size) override {}
        cw::WindowMetrics metrics() const override { return metrics_; }
        void show() override {}
        void requestClose() override { auto cb = on_close_requested; if (cb) cb(); }

        cw::WindowOptions              options;
        std::shared_ptr<cw::RenderBox> root_;
    private:
        cw::View          view_;
        cw::WindowMetrics metrics_;
    };

    // Records every window; posted tasks run when the test says so.
    class MpWindowing : public cw::PlatformWindowing
    {
    public:
        bool                                support_popups = true;
        std::vector<MpWindow*>              created;
        std::vector<std::function<void()>>  tasks;

        std::unique_ptr<cw::PlatformWindow> createWindow(const cw::WindowOptions& options) override
        {
            if (options.kind == cw::WindowKind::popup && !support_popups) return nullptr;
            auto w = std::make_unique<MpWindow>(options);
            created.push_back(w.get());
            return w;
        }
        void postTask(std::function<void()> task) override { tasks.push_back(std::move(task)); }
        void runTasks()
        {
            for (int round = 0; round < 4 && !tasks.empty(); ++round)
            {
                auto pending = std::move(tasks);
                tasks.clear();
                for (auto& t : pending) t();
            }
        }
    };

    struct MpPlatform
    {
        MpWindowing windowing;
        cw::View    main_view;
        MpPlatform()
        {
            main_view.makePrimary();
            cw::PlatformWindowing::setInstance(&windowing);
        }
        ~MpPlatform()
        {
            windowing.runTasks();
            cw::PlatformWindowing::setInstance(nullptr);
        }
    };

    // Mounts `child` in `view` under an Overlay (like an app's root) and
    // returns the root element.
    std::shared_ptr<cw::Element> mpMount(cw::View& view, cw::WidgetRef child)
    {
        cw::View::Scope scope(&view);
        auto overlay = std::make_shared<cw::Overlay>(
            std::vector<std::shared_ptr<cw::OverlayEntry>>{ std::make_shared<cw::OverlayEntry>(child) });
        auto root = std::make_shared<cw::MediaQuery>(cw::MediaQueryData{}, overlay)->createElement();
        root->mount(nullptr);
        return root;
    }

    // Lays out and paints the main tree (records the anchor's position).
    void mpPaint(cw::Element& root, float w = 400.0f, float h = 300.0f)
    {
        cw::Element::buildScope();
        auto* roe = root.findDescendantRenderObjectElement();
        ASSERT_NE(roe, nullptr);
        auto* box = dynamic_cast<cw::RenderBox*>(roe->renderObject());
        ASSERT_NE(box, nullptr);
        box->layout(cw::BoxConstraints::tight(w, h));
        cw::PaintContext ctx(w, h);
        box->paint(ctx, cw::Offset::zero());
    }

    cw::PointerEvent mpDown(float x, float y)
    {
        cw::PointerEvent e;
        e.kind       = cw::PointerEventKind::down;
        e.pointer_id = 9;
        e.position   = {x, y};
        e.pressure   = 1.0f;
        return e;
    }

    // An anchor: a 50x20 box at the window's top-left.
    cw::WidgetRef mpAnchor(bool open, int* dismissed)
    {
        auto anchor        = std::make_shared<cw::PopupWindowAnchor>();
        anchor->child      = std::make_shared<cw::SizedBox>(50.0f, 20.0f);
        anchor->popup      = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
        anchor->open       = open;
        anchor->popup_size = {120.0f, 80.0f};
        anchor->offset     = {0.0f, 4.0f};
        anchor->on_dismiss = [dismissed] { ++*dismissed; };
        return cw::Align::create(cw::Alignment::topLeft(), anchor);
    }
}

// ---------------------------------------------------------------------------
// Modal windows and owned windows (M2)
// ---------------------------------------------------------------------------

TEST(ModalWindow, BlocksTheParentsInputUntilDestroyed)
{
    MpPlatform platform;
    auto modal = cw::WindowController::create({.kind = cw::WindowKind::dialog, .modal = true});
    ASSERT_NE(modal, nullptr);
    EXPECT_TRUE(platform.main_view.isInputBlocked());
    EXPECT_EQ(platform.main_view.inputBlocker(), modal->view());
    EXPECT_TRUE(platform.main_view.pointerDispatcher().isInputBlocked());
    EXPECT_TRUE(platform.main_view.focusManager().isInputBlocked());
    EXPECT_FALSE(modal->view()->isInputBlocked()) << "the modal itself takes input";

    modal->destroy();
    platform.windowing.runTasks();
    EXPECT_FALSE(platform.main_view.isInputBlocked());
    EXPECT_FALSE(platform.main_view.pointerDispatcher().isInputBlocked());
    EXPECT_FALSE(platform.main_view.focusManager().isInputBlocked());
}

TEST(ModalWindow, NonModalDialogDoesNotBlock)
{
    MpPlatform platform;
    auto dialog = cw::WindowController::create({.kind = cw::WindowKind::dialog});
    EXPECT_FALSE(platform.main_view.isInputBlocked());
}

TEST(ModalWindow, ModalOverASecondaryWindowBlocksOnlyThatWindow)
{
    MpPlatform platform;
    auto inspector = cw::WindowController::create({});
    auto modal = cw::WindowController::create({.parent = inspector->platformWindow(), .modal = true});
    EXPECT_TRUE(inspector->view()->isInputBlocked());
    EXPECT_FALSE(platform.main_view.isInputBlocked());
}

TEST(ModalWindow, BlockedDispatcherDropsEventsButStillReportsDowns)
{
    cw::PointerDispatcher dispatcher;
    int downs = 0;
    dispatcher.addPointerDownObserver([&](const cw::PointerEvent&) { ++downs; });
    dispatcher.setInputBlocked(true);
    dispatcher.handlePointerEvent(mpDown(1.0f, 1.0f));
    EXPECT_EQ(downs, 1) << "observers see presses even while blocked";
}

TEST(OwnedWindow, ClosesWithItsOwner)
{
    MpPlatform platform;
    auto owner = cw::WindowController::create({});
    auto owned = cw::WindowController::create({.kind = cw::WindowKind::dialog,
                                               .parent = owner->platformWindow()});
    bool owned_destroyed = false;
    owned->on_destroyed = [&] { owned_destroyed = true; };

    owner->destroy();
    platform.windowing.runTasks();
    EXPECT_FALSE(owner->isOpen());
    EXPECT_TRUE(owned_destroyed);
    EXPECT_FALSE(owned->isOpen());
}

TEST(OwnedWindow, ForViewFindsTheController)
{
    MpPlatform platform;
    auto w = cw::WindowController::create({});
    EXPECT_EQ(cw::WindowController::forView(w->view()), w.get());
    EXPECT_EQ(cw::WindowController::forView(&platform.main_view), nullptr);
}

// ---------------------------------------------------------------------------
// Per-window overlays (M7)
// ---------------------------------------------------------------------------

TEST(WindowOverlay, InsertTargetsTheWindowBeingHandled)
{
    MpPlatform platform;
    cw::View other;
    auto main_root  = mpMount(platform.main_view, std::make_shared<cw::SizedBox>(1.0f, 1.0f));
    auto other_root = mpMount(other, std::make_shared<cw::SizedBox>(1.0f, 1.0f));
    auto* main_overlay  = cw::Overlay::forView(&platform.main_view);
    auto* other_overlay = cw::Overlay::forView(&other);
    ASSERT_NE(main_overlay, nullptr);
    ASSERT_NE(other_overlay, nullptr);
    ASSERT_NE(main_overlay, other_overlay);

    auto entry = std::make_shared<cw::OverlayEntry>(std::make_shared<cw::SizedBox>(1.0f, 1.0f));
    {
        cw::View::Scope scope(&other);  // e.g. a tap in that window
        cw::Overlay::insert(entry);
    }
    EXPECT_EQ(other_overlay->entries.size(), 2u);
    EXPECT_EQ(main_overlay->entries.size(), 1u);

    cw::Overlay::remove(entry);  // from wherever it is
    EXPECT_EQ(other_overlay->entries.size(), 1u);
    other_root->unmount();
    main_root->unmount();
}

TEST(WindowOverlay, EveryHostedWindowHasItsOwnOverlay)
{
    MpPlatform platform;
    auto w = cw::WindowController::create({.size = {100.0f, 100.0f}});
    auto root = mpMount(platform.main_view, std::make_shared<cw::Window>(
        w, std::make_shared<cw::SizedBox>(5.0f, 5.0f)));
    EXPECT_NE(cw::Overlay::forView(w->view()), nullptr);
    EXPECT_NE(cw::Overlay::forView(w->view()), cw::Overlay::forView(&platform.main_view));
    root->unmount();
}

// ---------------------------------------------------------------------------
// PopupWindowAnchor (M7)
// ---------------------------------------------------------------------------

TEST(PopupWindowAnchor, OpensANativePopupBelowTheAnchorOncePainted)
{
    MpPlatform platform;
    int dismissed = 0;
    auto root = mpMount(platform.main_view, mpAnchor(true, &dismissed));
    EXPECT_TRUE(platform.windowing.created.empty()) << "position unknown before the first paint";

    mpPaint(*root);
    platform.windowing.runTasks();
    ASSERT_EQ(platform.windowing.created.size(), 1u);
    const cw::WindowOptions& o = platform.windowing.created[0]->options;
    EXPECT_EQ(o.kind, cw::WindowKind::popup);
    EXPECT_EQ(o.parent, nullptr) << "the main window";
    ASSERT_TRUE(o.position.has_value());
    EXPECT_FLOAT_EQ(o.position->x, 0.0f);
    EXPECT_FLOAT_EQ(o.position->y, 24.0f);  // anchor height 20 + offset 4
    EXPECT_FLOAT_EQ(o.size.width, 120.0f);
    root->unmount();
}

TEST(PopupWindowAnchor, PressOutsideDismissesButAPressOnTheAnchorDoesNot)
{
    MpPlatform platform;
    int dismissed = 0;
    auto root = mpMount(platform.main_view, mpAnchor(true, &dismissed));
    mpPaint(*root);
    platform.windowing.runTasks();
    ASSERT_EQ(platform.windowing.created.size(), 1u);

    // On the anchor (e.g. its own toggle button): left to the anchor.
    platform.main_view.pointerDispatcher().handlePointerEvent(mpDown(10.0f, 10.0f));
    EXPECT_EQ(dismissed, 0);
    // Anywhere else in the parent window.
    platform.main_view.pointerDispatcher().handlePointerEvent(mpDown(300.0f, 200.0f));
    EXPECT_EQ(dismissed, 1);
    root->unmount();
}

TEST(PopupWindowAnchor, LosingFocusDismisses)
{
    MpPlatform platform;
    int dismissed = 0;
    auto root = mpMount(platform.main_view, mpAnchor(true, &dismissed));
    mpPaint(*root);
    platform.windowing.runTasks();
    ASSERT_EQ(platform.windowing.created.size(), 1u);
    platform.windowing.created[0]->requestClose();  // what the platform does on resign-key
    EXPECT_EQ(dismissed, 1);
    root->unmount();
}

TEST(PopupWindowAnchor, FallsBackToTheWindowsOverlayWithoutNativePopups)
{
    MpPlatform platform;
    platform.windowing.support_popups = false;  // e.g. iOS, Android
    int dismissed = 0;
    auto root = mpMount(platform.main_view, mpAnchor(true, &dismissed));
    mpPaint(*root);
    platform.windowing.runTasks();

    EXPECT_TRUE(platform.windowing.created.empty());
    auto* overlay = cw::Overlay::forView(&platform.main_view);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->entries.size(), 2u) << "the popup entry above the app";

    // A press inside the in-window popup is not "outside".
    platform.main_view.pointerDispatcher().handlePointerEvent(mpDown(20.0f, 40.0f));
    EXPECT_EQ(dismissed, 0);
    platform.main_view.pointerDispatcher().handlePointerEvent(mpDown(350.0f, 250.0f));
    EXPECT_EQ(dismissed, 1);
    root->unmount();
}
