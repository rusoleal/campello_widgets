// View: per-window subsystems and owner resolution (Phase 17, M1).
//
// Covers the rules multi-window correctness rests on:
//   - render objects and elements belong to their *parent's* window; only
//     roots take the ambient window (View::Scope / primary),
//   - invalidations reach the owning window's frame hook, not every window's,
//   - during dispatch, PointerDispatcher::activeDispatcher() is the
//     dispatcher delivering the event,
//   - with no View at all, the legacy process-wide fallbacks behave as before.

#include <gtest/gtest.h>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // First descendant element (depth-first) for which `pred` holds.
    cw::Element* viewTestFindElement(cw::Element* root, const std::function<bool(cw::Element*)>& pred)
    {
        if (!root) return nullptr;
        if (pred(root)) return root;
        cw::Element* found = nullptr;
        root->visitChildren([&](cw::Element* child) {
            if (!found) found = viewTestFindElement(child, pred);
        });
        return found;
    }
}

// ---------------------------------------------------------------------------
// Registry and ambient scope
// ---------------------------------------------------------------------------

TEST(View, RegistryTracksLiveViews)
{
    cw::View* raw = nullptr;
    {
        cw::View a;
        raw = &a;
        EXPECT_TRUE(cw::View::isAlive(&a));
        auto all = cw::View::all();
        EXPECT_NE(std::find(all.begin(), all.end(), &a), all.end());
    }
    EXPECT_FALSE(cw::View::isAlive(raw));
    EXPECT_FALSE(cw::View::isAlive(nullptr));
}

TEST(View, ScopesNestAndRestore)
{
    cw::View a, b;
    EXPECT_EQ(cw::View::scoped(), nullptr);
    {
        cw::View::Scope sa(&a);
        EXPECT_EQ(cw::View::current(), &a);
        {
            cw::View::Scope sb(&b);
            EXPECT_EQ(cw::View::current(), &b);
            EXPECT_EQ(cw::View::scoped(), &b);
        }
        EXPECT_EQ(cw::View::current(), &a);
    }
    EXPECT_EQ(cw::View::scoped(), nullptr);
}

TEST(View, MakePrimaryInstallsAndClearsLegacyFallbacks)
{
    {
        cw::View v;
        v.makePrimary();
        EXPECT_EQ(cw::View::primary(), &v);
        EXPECT_EQ(cw::View::current(), &v);  // no scope: primary
        EXPECT_EQ(cw::PointerDispatcher::activeDispatcher(), &v.pointerDispatcher());
        EXPECT_EQ(cw::FocusManager::activeManager(), &v.focusManager());
        EXPECT_EQ(cw::TextInputManager::activeManager(), &v.textInputManager());
    }
    EXPECT_EQ(cw::View::primary(), nullptr);
    EXPECT_EQ(cw::PointerDispatcher::activeDispatcher(), nullptr);
    EXPECT_EQ(cw::FocusManager::activeManager(), nullptr);
    EXPECT_EQ(cw::TextInputManager::activeManager(), nullptr);
}

TEST(View, ScopedViewWinsOverProcessWideManagers)
{
    cw::View primary, other;
    primary.makePrimary();
    cw::View::Scope scope(&other);
    EXPECT_EQ(cw::PointerDispatcher::activeDispatcher(), &other.pointerDispatcher());
    EXPECT_EQ(cw::FocusManager::activeManager(), &other.focusManager());
    EXPECT_EQ(cw::TextInputManager::activeManager(), &other.textInputManager());
}

// ---------------------------------------------------------------------------
// RenderObject ownership
// ---------------------------------------------------------------------------

TEST(View, RenderObjectOutsideAnyViewHasNoOwnerAndUsesFallback)
{
    cw::PointerDispatcher test_dispatcher;
    cw::PointerDispatcher::setActiveDispatcher(&test_dispatcher);
    cw::RenderSizedBox box;
    EXPECT_EQ(box.view(), nullptr);
    EXPECT_EQ(box.pointerDispatcher(), &test_dispatcher);
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(View, RootRenderObjectTakesAmbientView)
{
    cw::View a;
    cw::View::Scope scope(&a);
    cw::RenderSizedBox box;
    EXPECT_EQ(box.view(), &a);
    EXPECT_EQ(box.pointerDispatcher(), &a.pointerDispatcher());
}

TEST(View, ChildInheritsParentsViewOverAmbient)
{
    // Window A's frame can rebuild window B's elements (one shared element
    // tree), so a render object created while A is ambient but inserted
    // under B's tree must belong to B.
    cw::View a, b;
    std::shared_ptr<cw::RenderPadding> parent_in_b;
    {
        cw::View::Scope sb(&b);
        parent_in_b = std::make_shared<cw::RenderPadding>();
    }
    cw::View::Scope sa(&a);
    auto child = std::make_shared<cw::RenderSizedBox>();
    EXPECT_EQ(child->view(), &a);
    parent_in_b->setChild(child);
    EXPECT_EQ(child->view(), &b);
    EXPECT_EQ(child->pointerDispatcher(), &b.pointerDispatcher());
}

TEST(View, OwnerIsForgottenOnceViewIsDestroyed)
{
    std::shared_ptr<cw::RenderSizedBox> box;
    {
        cw::View a;
        cw::View::Scope scope(&a);
        box = std::make_shared<cw::RenderSizedBox>();
        EXPECT_EQ(box->view(), &a);
    }
    EXPECT_EQ(box->view(), nullptr);  // no dangling owner
}

TEST(View, InvalidationRequestsAFrameOnlyForTheOwningWindow)
{
    cw::View a, b;
    int frames_a = 0, frames_b = 0;
    a.setFrameRequestCallback([&] { ++frames_a; });
    b.setFrameRequestCallback([&] { ++frames_b; });

    std::shared_ptr<cw::RenderSizedBox> box_b;
    {
        cw::View::Scope sb(&b);
        box_b = std::make_shared<cw::RenderSizedBox>();
    }
    // Lay out and paint once so the dirty flags are clean (a fresh render
    // object starts with needs_paint_ set, which makes markNeedsPaint() a
    // no-op until it has painted).
    box_b->layout(cw::BoxConstraints::loose(10.0f, 10.0f));
    cw::PaintContext ctx(10.0f, 10.0f);
    box_b->paint(ctx, cw::Offset::zero());
    frames_a = frames_b = 0;

    box_b->markNeedsPaint();
    EXPECT_EQ(frames_b, 1);
    EXPECT_EQ(frames_a, 0);
}

// ---------------------------------------------------------------------------
// Dispatch scope
// ---------------------------------------------------------------------------

TEST(View, ActiveDispatcherIsTheOneDeliveringTheTick)
{
    cw::View a, b;
    a.makePrimary();  // the process-wide fallback is A's...
    cw::RenderSizedBox box;
    cw::PointerDispatcher* seen = nullptr;
    b.pointerDispatcher().addTickHandler(&box, [&](uint64_t) {
        seen = cw::PointerDispatcher::activeDispatcher();
    });
    b.pointerDispatcher().tick(1);  // ...but B is delivering this tick
    EXPECT_EQ(seen, &b.pointerDispatcher());
    b.pointerDispatcher().removeTickHandler(&box);
    // Outside dispatch the fallback applies again.
    EXPECT_EQ(cw::PointerDispatcher::activeDispatcher(), &a.pointerDispatcher());
}

// ---------------------------------------------------------------------------
// Element ownership
// ---------------------------------------------------------------------------

TEST(View, ElementsInheritTheRootElementsView)
{
    cw::View a, b;
    auto tree = std::make_shared<cw::Padding>(cw::EdgeInsets::all(4.0f),
                                              std::make_shared<cw::SizedBox>(10.0f, 10.0f));
    std::shared_ptr<cw::Element> root;
    {
        cw::View::Scope sa(&a);
        root = tree->createElement();
        root->mount(nullptr);
    }
    EXPECT_EQ(root->view(), &a);
    EXPECT_EQ(cw::View::of(*root), &a);

    // A descendant mounted later, while another window is ambient, still
    // belongs to its parent's window.
    cw::Element* leaf = viewTestFindElement(root.get(), [&](cw::Element* e) { return e != root.get(); });
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->view(), &a);

    cw::View::Scope sb(&b);
    auto extra = std::make_shared<cw::SizedBox>(1.0f, 1.0f)->createElement();
    extra->mount(root.get());
    EXPECT_EQ(extra->view(), &a);
    extra->unmount();
    root->unmount();
}
