#include <gtest/gtest.h>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_traversal_policy.hpp>
#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/widgets/focus_scope.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Installs `m` as FocusManager::activeManager() for the scope's
    // lifetime and restores whatever was active before -- keeps each test
    // isolated from state any other test left behind, mirroring how real
    // platform startup code sets the active manager once at launch.
    class ScopedActiveManager
    {
    public:
        explicit ScopedActiveManager(cw::FocusManager& m)
            : previous_(cw::FocusManager::activeManager())
        {
            cw::FocusManager::setActiveManager(&m);
        }
        ~ScopedActiveManager() { cw::FocusManager::setActiveManager(previous_); }

    private:
        cw::FocusManager* previous_;
    };

    cw::KeyEvent keyDown(cw::KeyCode code)
    {
        cw::KeyEvent e;
        e.kind     = cw::KeyEventKind::down;
        e.key_code = code;
        return e;
    }

    // A RenderFocus that's laid out to a fixed size and painted at a fixed
    // offset gets its FocusNode::bounds() populated for real via the
    // production performPaint() -- used by the ReadingOrderTraversalPolicy
    // tests below, since bounds_ is intentionally private with no test
    // seam other than the real paint path.
    void paintAt(cw::RenderFocus& box, float x, float y, float size = 10.0f)
    {
        box.layout(cw::BoxConstraints::tight(size, size));
        cw::PaintContext ctx(800.0f, 800.0f);
        box.paint(ctx, cw::Offset{x, y});
    }
} // namespace

// -----------------------------------------------------------------------
// Basic registration / focus control
// -----------------------------------------------------------------------

TEST(FocusManager, RequestFocusGivesNodeFocus)
{
    cw::FocusManager mgr;
    cw::FocusNode node;
    mgr.registerNode(&node);

    mgr.requestFocus(&node);
    EXPECT_TRUE(node.hasFocus());
}

TEST(FocusManager, UnfocusClearsFocus)
{
    cw::FocusManager mgr;
    cw::FocusNode node;
    mgr.registerNode(&node);
    mgr.requestFocus(&node);

    mgr.unfocus(&node);
    EXPECT_FALSE(node.hasFocus());
}

TEST(FocusManager, CanRequestFocusFalsePreventsFocus)
{
    cw::FocusManager mgr;
    cw::FocusNode node;
    node.setCanRequestFocus(false);
    mgr.registerNode(&node);

    mgr.requestFocus(&node);
    EXPECT_FALSE(node.hasFocus());
}

TEST(FocusManager, UnregisterClearsFocusAndRemovesFromOrder)
{
    cw::FocusManager mgr;
    cw::FocusNode a, b;
    mgr.registerNode(&a);
    mgr.registerNode(&b);
    mgr.requestFocus(&a);

    mgr.unregisterNode(&a);
    EXPECT_FALSE(a.hasFocus());

    // b is still the only registered node -- forward from nothing lands on it.
    mgr.moveFocusForward();
    EXPECT_TRUE(b.hasFocus());
}

// -----------------------------------------------------------------------
// Tab traversal (flat, no scopes)
// -----------------------------------------------------------------------

TEST(FocusManager, MoveFocusForwardWrapsAround)
{
    cw::FocusManager mgr;
    cw::FocusNode a, b, c;
    mgr.registerNode(&a);
    mgr.registerNode(&b);
    mgr.registerNode(&c);

    mgr.requestFocus(&a);
    mgr.moveFocusForward();
    EXPECT_TRUE(b.hasFocus());
    mgr.moveFocusForward();
    EXPECT_TRUE(c.hasFocus());
    mgr.moveFocusForward();
    EXPECT_TRUE(a.hasFocus()); // wraps
}

TEST(FocusManager, MoveFocusBackwardWrapsAround)
{
    cw::FocusManager mgr;
    cw::FocusNode a, b, c;
    mgr.registerNode(&a);
    mgr.registerNode(&b);
    mgr.registerNode(&c);

    mgr.requestFocus(&a);
    mgr.moveFocusBackward();
    EXPECT_TRUE(c.hasFocus()); // wraps
    mgr.moveFocusBackward();
    EXPECT_TRUE(b.hasFocus());
}

TEST(FocusManager, SkipTraversalExcludedFromTabOrder)
{
    cw::FocusManager mgr;
    cw::FocusNode a, b, c;
    b.setSkipTraversal(true);
    mgr.registerNode(&a);
    mgr.registerNode(&b);
    mgr.registerNode(&c);

    mgr.requestFocus(&a);
    mgr.moveFocusForward();
    EXPECT_TRUE(c.hasFocus()); // skips b entirely
}

TEST(FocusManager, CanRequestFocusFalseExcludedFromTabOrder)
{
    cw::FocusManager mgr;
    cw::FocusNode a, b, c;
    b.setCanRequestFocus(false);
    mgr.registerNode(&a);
    mgr.registerNode(&b);
    mgr.registerNode(&c);

    mgr.requestFocus(&a);
    mgr.moveFocusForward();
    EXPECT_TRUE(c.hasFocus());
}

// -----------------------------------------------------------------------
// Ancestor bubbling (dispatchToFocusChain, exercised via handleKeyEvent)
// -----------------------------------------------------------------------

TEST(FocusManager, KeyEventBubblesToAncestorWhenLeafDoesNotConsume)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    auto ancestorNode = std::make_shared<cw::FocusNode>();
    auto leafNode      = std::make_shared<cw::FocusNode>();

    cw::RenderFocus ancestorBox;
    cw::RenderFocus leafBox;
    ancestorBox.focus_node = ancestorNode;
    leafBox.focus_node     = leafNode;
    leafBox.setParent(&ancestorBox); // triggers attach() -> setOwner()

    bool ancestorSawKey = false;
    ancestorNode->on_key = [&](const cw::KeyEvent&) { ancestorSawKey = true; return true; };
    leafNode->on_key      = [](const cw::KeyEvent&) { return false; }; // doesn't consume

    mgr.registerNode(ancestorNode.get());
    mgr.registerNode(leafNode.get());
    mgr.requestFocus(leafNode.get());

    mgr.handleKeyEvent(keyDown(cw::KeyCode::a));
    EXPECT_TRUE(ancestorSawKey);

    leafBox.setParent(nullptr);
}

TEST(FocusManager, KeyEventStopsAtConsumingLeaf)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    auto ancestorNode = std::make_shared<cw::FocusNode>();
    auto leafNode      = std::make_shared<cw::FocusNode>();

    cw::RenderFocus ancestorBox;
    cw::RenderFocus leafBox;
    ancestorBox.focus_node = ancestorNode;
    leafBox.focus_node     = leafNode;
    leafBox.setParent(&ancestorBox);

    bool ancestorSawKey = false;
    ancestorNode->on_key = [&](const cw::KeyEvent&) { ancestorSawKey = true; return true; };
    leafNode->on_key      = [](const cw::KeyEvent&) { return true; }; // consumes

    mgr.registerNode(ancestorNode.get());
    mgr.registerNode(leafNode.get());
    mgr.requestFocus(leafNode.get());

    mgr.handleKeyEvent(keyDown(cw::KeyCode::a));
    EXPECT_FALSE(ancestorSawKey);

    leafBox.setParent(nullptr);
}

// -----------------------------------------------------------------------
// Scope containment (FocusNode::isScope())
// -----------------------------------------------------------------------

TEST(FocusManager, NearestEnclosingScopeChecksNodeItselfFirst)
{
    cw::FocusNode scope;
    scope.setScope(true);
    EXPECT_EQ(cw::FocusManager::nearestEnclosingScope(&scope), &scope);
}

TEST(FocusManager, NearestEnclosingScopeNullForUnscopedNode)
{
    cw::FocusNode plain;
    EXPECT_EQ(cw::FocusManager::nearestEnclosingScope(&plain), nullptr);
    EXPECT_EQ(cw::FocusManager::nearestEnclosingScope(nullptr), nullptr);
}

TEST(FocusManager, TraversalStaysWithinScope)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    cw::FocusNode a; // outside any scope

    auto scopeNode = std::make_shared<cw::FocusNode>();
    scopeNode->setScope(true);
    cw::RenderFocus scopeBox;
    scopeBox.focus_node = scopeNode;

    auto bNode = std::make_shared<cw::FocusNode>();
    auto cNode = std::make_shared<cw::FocusNode>();
    cw::RenderFocus bBox, cBox;
    bBox.focus_node = bNode;
    cBox.focus_node = cNode;
    bBox.setParent(&scopeBox);
    cBox.setParent(&scopeBox);

    mgr.registerNode(&a);
    mgr.registerNode(scopeNode.get());
    mgr.registerNode(bNode.get());
    mgr.registerNode(cNode.get());

    mgr.requestFocus(bNode.get());
    mgr.moveFocusForward();
    EXPECT_TRUE(cNode->hasFocus()); // b -> c, staying inside the scope

    mgr.moveFocusForward();
    EXPECT_TRUE(bNode->hasFocus()); // wraps back to b, never reaches `a`

    bBox.setParent(nullptr);
    cBox.setParent(nullptr);
}

TEST(FocusManager, ScopeCloseRestoresPreviousFocus)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    cw::FocusNode a;
    mgr.registerNode(&a);
    mgr.requestFocus(&a);

    {
        auto scopeNode = std::make_shared<cw::FocusNode>();
        scopeNode->setScope(true);
        cw::RenderFocus scopeBox;
        scopeBox.focus_node = scopeNode;

        auto bNode = std::make_shared<cw::FocusNode>();
        cw::RenderFocus bBox;
        bBox.focus_node = bNode;
        bBox.setParent(&scopeBox);

        mgr.registerNode(scopeNode.get()); // captures `a` as previouslyFocusedOutside
        mgr.registerNode(bNode.get());
        mgr.requestFocus(bNode.get());
        EXPECT_TRUE(bNode->hasFocus());

        // Scope unregisters BEFORE its focused descendant, and the
        // ancestor link (bBox -> scopeBox) stays intact the whole time --
        // this is not an arbitrary choice, it's the order/state
        // production code actually guarantees: a removed subtree's
        // RenderObject graph stays fully linked (owning each other via
        // shared_ptr child_ members, never individually detach()ed) until
        // a single release at the tree boundary triggers a normal
        // top-down member-destructor cascade, so a scope's own destructor
        // (and thus its unregisterNode() call) always runs before a
        // descendant's, while the descendant's ancestor chain is still
        // walkable -- see the design note above
        // FocusManager::unregisterNode()'s implementation. Explicitly
        // detaching bBox from scopeBox before closing the scope (as an
        // earlier version of this test did) simulates a teardown order
        // that can't actually happen and defeats the very check being
        // tested.
        mgr.unregisterNode(scopeNode.get()); // scope closes while focus still inside it
        mgr.unregisterNode(bNode.get());
        bBox.setParent(nullptr);
    }

    EXPECT_TRUE(a.hasFocus());
}

TEST(FocusManager, ScopeCloseDoesNotClobberFocusMovedElsewhereFirst)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    cw::FocusNode a;
    mgr.registerNode(&a);
    mgr.requestFocus(&a);

    auto scopeNode = std::make_shared<cw::FocusNode>();
    scopeNode->setScope(true);
    mgr.registerNode(scopeNode.get());
    mgr.requestFocus(scopeNode.get());

    // Focus explicitly moved back to `a` before the scope closes -- it's
    // no longer "trapped" inside the scope, so closing it must not
    // re-request previouslyFocusedOutside() (which also happens to be
    // `a` here, but the point is the mechanism shouldn't fire at all).
    mgr.requestFocus(&a);
    mgr.unregisterNode(scopeNode.get());

    EXPECT_TRUE(a.hasFocus());
}

TEST(FocusManager, DanglingPreviouslyFocusedOutsideClearedOnUnregister)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    auto a = std::make_shared<cw::FocusNode>();
    mgr.registerNode(a.get());
    mgr.requestFocus(a.get());

    auto scopeNode = std::make_shared<cw::FocusNode>();
    scopeNode->setScope(true);
    mgr.registerNode(scopeNode.get()); // captures `a`
    mgr.requestFocus(scopeNode.get());

    // `a`'s owner is destroyed/unregistered while the scope is still open.
    mgr.unregisterNode(a.get());
    EXPECT_EQ(scopeNode->previouslyFocusedOutside(), nullptr);

    // Closing the scope must not crash or resurrect the dangling pointer.
    mgr.unregisterNode(scopeNode.get());
    SUCCEED();
}

TEST(FocusManager, NestedScopeRestoresToOuterScopeNotRoot)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    cw::FocusNode a; // root, unscoped
    mgr.registerNode(&a);
    mgr.requestFocus(&a);

    // Real ancestor wiring throughout: s2 nested inside b, b nested
    // inside s1 -- nearestEnclosingScope() only means anything when the
    // FocusNode::parent() chain (owner_->RenderObject::parent()) is
    // actually linked, which plain registerNode() calls alone don't do.
    auto s1Node = std::make_shared<cw::FocusNode>();
    s1Node->setScope(true);
    cw::RenderFocus s1Box;
    s1Box.focus_node = s1Node;

    auto bNode = std::make_shared<cw::FocusNode>();
    cw::RenderFocus bBox;
    bBox.focus_node = bNode;
    bBox.setParent(&s1Box);

    auto s2Node = std::make_shared<cw::FocusNode>();
    s2Node->setScope(true);
    cw::RenderFocus s2Box;
    s2Box.focus_node = s2Node;
    s2Box.setParent(&bBox);

    mgr.registerNode(s1Node.get()); // captures `a`
    mgr.requestFocus(s1Node.get());

    mgr.registerNode(bNode.get());
    mgr.requestFocus(bNode.get());  // focus moves into s1, onto b

    mgr.registerNode(s2Node.get()); // captures `b` (current_focus_ right now)
    mgr.requestFocus(s2Node.get());

    // Inner scope closes first (see ScopeCloseRestoresPreviousFocus's
    // ordering note) -- restores to b, its own pre-open focus target,
    // not all the way back to a.
    mgr.unregisterNode(s2Node.get());
    EXPECT_TRUE(bNode->hasFocus());

    // Outer scope closes -- restores to a.
    mgr.unregisterNode(s1Node.get());
    EXPECT_TRUE(a.hasFocus());

    mgr.unregisterNode(bNode.get());
    s2Box.setParent(nullptr);
    bBox.setParent(nullptr);
}

// -----------------------------------------------------------------------
// Traversal policies
// -----------------------------------------------------------------------

TEST(FocusTraversalPolicy, OrderedPreservesInputOrder)
{
    cw::FocusNode a, b, c;
    std::vector<cw::FocusNode*> candidates{&a, &b, &c};

    cw::OrderedTraversalPolicy policy;
    EXPECT_EQ(policy.order(candidates), candidates);
}

TEST(FocusTraversalPolicy, ReadingOrderSortsTopToBottomThenLeftToRight)
{
    cw::RenderFocus topLeft, topRight, bottom;
    auto topLeftNode  = std::make_shared<cw::FocusNode>();
    auto topRightNode = std::make_shared<cw::FocusNode>();
    auto bottomNode   = std::make_shared<cw::FocusNode>();
    topLeft.focus_node  = topLeftNode;
    topRight.focus_node = topRightNode;
    bottom.focus_node   = bottomNode;

    // Registered/painted out of visual order on purpose.
    paintAt(bottom,   0.0f, 100.0f);
    paintAt(topRight, 100.0f, 0.0f);
    paintAt(topLeft,  0.0f, 0.0f);

    std::vector<cw::FocusNode*> candidates{bottomNode.get(), topRightNode.get(), topLeftNode.get()};

    cw::ReadingOrderTraversalPolicy policy;
    std::vector<cw::FocusNode*> ordered = policy.order(candidates);

    ASSERT_EQ(ordered.size(), 3u);
    EXPECT_EQ(ordered[0], topLeftNode.get());
    EXPECT_EQ(ordered[1], topRightNode.get());
    EXPECT_EQ(ordered[2], bottomNode.get());
}

TEST(FocusManager, ScopeWithReadingOrderPolicyUsesItForTabOrder)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    auto scopeNode = std::make_shared<cw::FocusNode>();
    scopeNode->setScope(true);
    scopeNode->traversal_policy = std::make_shared<cw::ReadingOrderTraversalPolicy>();
    cw::RenderFocus scopeBox;
    scopeBox.focus_node = scopeNode;

    cw::RenderFocus bottomBox, topBox;
    auto bottomNode = std::make_shared<cw::FocusNode>();
    auto topNode    = std::make_shared<cw::FocusNode>();
    bottomBox.focus_node = bottomNode;
    topBox.focus_node    = topNode;
    bottomBox.setParent(&scopeBox);
    topBox.setParent(&scopeBox);

    paintAt(bottomBox, 0.0f, 100.0f);
    paintAt(topBox,    0.0f, 0.0f);

    mgr.registerNode(scopeNode.get());
    // Registered in "wrong" (bottom-then-top) order -- ReadingOrder should
    // still visit top first.
    mgr.registerNode(bottomNode.get());
    mgr.registerNode(topNode.get());

    mgr.requestFocus(topNode.get());
    mgr.moveFocusForward();
    EXPECT_TRUE(bottomNode->hasFocus());

    bottomBox.setParent(nullptr);
    topBox.setParent(nullptr);
}

// -----------------------------------------------------------------------
// Focus / FocusScope widgets
// -----------------------------------------------------------------------

TEST(Focus, ScopePropSetsIsScopeOnFocusNodeBeforeRegistering)
{
    cw::FocusManager mgr;
    ScopedActiveManager active(mgr);

    cw::Focus widget;
    widget.focus_node = std::make_shared<cw::FocusNode>();
    widget.scope       = true;

    auto ro = widget.createRenderObject();
    EXPECT_TRUE(widget.focus_node->isScope());

    mgr.unregisterNode(widget.focus_node.get());
    (void)ro;
}

TEST(FocusScope, AutoCreatesItsOwnScopedFocusNode)
{
    cw::FocusScope scope;
    ASSERT_NE(scope.focus_node, nullptr);
    EXPECT_TRUE(scope.focus_node->isScope());
    EXPECT_TRUE(scope.scope);
}
