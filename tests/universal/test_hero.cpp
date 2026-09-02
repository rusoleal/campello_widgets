#include <gtest/gtest.h>
#include <algorithm>
#include <campello_widgets/widgets/hero.hpp>
#include <campello_widgets/widgets/navigator.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/ui/render_hero.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/post_frame_callbacks.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Mounts `root_widget` as a real Element tree (mirrors
    // TabWidgetsTest::mountAndLayout's own root-mounting idiom in
    // test_tabs_and_animated_switcher.cpp, minus the render-layout half --
    // these tests only need the Element tree itself, no RenderObject layout).
    // Deliberately distinctly-named from other test files' own root-mounting
    // helpers to avoid a Unity Build redefinition collision.
    std::shared_ptr<cw::Element> mountHeroTestTree(cw::WidgetRef root_widget)
    {
        auto element = root_widget->createElement();
        element->mount(nullptr);
        return element;
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Element::visitAllDescendants() -- single-child chains
// ---------------------------------------------------------------------------

TEST(ElementVisitAllDescendants, SingleChildChainVisitsEveryDescendantOnce)
{
    // Padding(Center(Padding(SizedBox))) -- a chain of single-child widgets.
    // SizedBox (a plain SingleChildRenderObjectWidget, not a composite
    // StatelessWidget) is used as the leaf specifically so this test's
    // element count is exactly predictable -- Container/Text are themselves
    // StatelessWidgets that build into further internal elements, which
    // would make an exact count assertion here test their own unrelated
    // internal composition rather than visitAllDescendants() itself
    // (discovered by running this test: an earlier draft using Container/
    // Text produced more elements than expected for exactly this reason).
    auto leaf      = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto inner_pad = std::make_shared<cw::Padding>();
    inner_pad->child = leaf;
    auto center   = std::make_shared<cw::Center>(inner_pad);
    auto outer_pad = std::make_shared<cw::Padding>();
    outer_pad->child = center;

    auto root = mountHeroTestTree(outer_pad);

    std::vector<cw::Element*> visited;
    cw::Element::visitAllDescendants(root.get(), [&](cw::Element* e) { visited.push_back(e); });

    // root itself (outer_pad's element) must never appear in the list.
    for (cw::Element* e : visited)
        EXPECT_NE(e, root.get());

    // 3 descendants: center's element, inner_pad's element, leaf's element.
    EXPECT_EQ(visited.size(), 3u);

    // No duplicates.
    std::vector<cw::Element*> unique_visited = visited;
    std::sort(unique_visited.begin(), unique_visited.end());
    unique_visited.erase(std::unique(unique_visited.begin(), unique_visited.end()), unique_visited.end());
    EXPECT_EQ(unique_visited.size(), visited.size());

    root->unmount();
}

// ---------------------------------------------------------------------------
// 2. Element::visitAllDescendants() -- multi-child (real Row/Column tree)
// ---------------------------------------------------------------------------

TEST(ElementVisitAllDescendants, MultiChildTreeVisitsEveryBranch)
{
    // Column [ Row [ SizedBox, SizedBox ], SizedBox ] -- a real multi-child
    // tree crossing two different multi-child element types. SizedBox (not
    // Container -- see the single-child test's own comment on why) keeps
    // the element count exactly predictable, one element per leaf.
    auto row_child_a = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto row_child_b = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto row = std::make_shared<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ row_child_a, row_child_b });

    auto column_child_b = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ row, column_child_b });

    auto root = mountHeroTestTree(column);

    int visit_count = 0;
    cw::Element::visitAllDescendants(root.get(), [&](cw::Element*) { ++visit_count; });

    // row's element + row_child_a's + row_child_b's + column_child_b's = 4
    // descendants beneath column's own element (root).
    EXPECT_EQ(visit_count, 4);

    root->unmount();
}

// ---------------------------------------------------------------------------
// 3-6. Hero::collectHeroesFor()
// ---------------------------------------------------------------------------

TEST(Hero, CollectHeroesForFindsHeroesAtDifferentNestingDepths)
{
    auto hero_a = std::make_shared<cw::Hero>();
    hero_a->tag   = "a";
    hero_a->child = std::make_shared<cw::Container>();

    auto hero_b = std::make_shared<cw::Hero>();
    hero_b->tag   = "b";
    hero_b->child = std::make_shared<cw::Container>();

    // hero_b sits inside a Row, nested two levels deeper than hero_a.
    auto row = std::make_shared<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_b });
    auto padded_row = std::make_shared<cw::Padding>();
    padded_row->child = row;

    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_a, padded_row });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());

    ASSERT_EQ(heroes.size(), 2u);
    ASSERT_TRUE(heroes.count("a"));
    ASSERT_TRUE(heroes.count("b"));
    EXPECT_TRUE(dynamic_cast<const cw::Hero*>(&heroes["a"]->widget()));
    EXPECT_EQ(static_cast<const cw::Hero&>(heroes["a"]->widget()).tag, "a");
    EXPECT_EQ(static_cast<const cw::Hero&>(heroes["b"]->widget()).tag, "b");

    root->unmount();
}

TEST(Hero, CollectHeroesForReturnsEmptyMapWhenNoHeroesPresent)
{
    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ std::make_shared<cw::Container>(), std::make_shared<cw::Text>("plain") });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());
    EXPECT_TRUE(heroes.empty());

    root->unmount();
}

TEST(Hero, CollectHeroesForWithDuplicateTagDoesNotCrashAndProducesOneEntry)
{
    // Two Heroes sharing the tag "dup" in one subtree -- unspecified which
    // one wins (last-visited, per Element::visitChildren()'s own traversal
    // order), but must not crash and must produce exactly one map entry.
    auto hero_1 = std::make_shared<cw::Hero>();
    hero_1->tag   = "dup";
    hero_1->child = std::make_shared<cw::Container>();

    auto hero_2 = std::make_shared<cw::Hero>();
    hero_2->tag   = "dup";
    hero_2->child = std::make_shared<cw::Container>();

    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_1, hero_2 });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());

    // Observed: exactly one entry survives (the later sibling, matching
    // Column's own child registration/visitChildren order) -- documented
    // here rather than silently assumed.
    ASSERT_EQ(heroes.size(), 1u);
    ASSERT_TRUE(heroes.count("dup"));

    root->unmount();
}

// Hero is a SingleChildRenderObjectWidget (Stage 5, mirroring Opacity/
// RenderOpacity), not a StatelessWidget passthrough -- so the "does the
// child come through unchanged" question is now answered at the render
// level: createRenderObject() must be a RenderHero, and mounting must wire
// `child` in as that RenderHero's own child.
TEST(Hero, CreateRenderObjectProducesARenderHeroWiredToItsChild)
{
    cw::Hero hero;
    hero.tag   = "x";
    hero.child = std::make_shared<cw::SizedBox>(10.0f, 10.0f);

    auto ro = hero.createRenderObject();
    auto* render_hero = dynamic_cast<cw::RenderHero*>(ro.get());
    ASSERT_NE(render_hero, nullptr);
    EXPECT_EQ(render_hero->child(), nullptr); // wiring happens via the Element, not the widget alone

    auto root = mountHeroTestTree(std::make_shared<cw::Hero>(hero));
    auto* hero_re = root->nearestRenderObjectElement();
    ASSERT_NE(hero_re, nullptr);
    auto* mounted_hero = dynamic_cast<cw::RenderHero*>(hero_re->renderObject());
    ASSERT_NE(mounted_hero, nullptr);
    EXPECT_NE(mounted_hero->child(), nullptr);

    root->unmount();
}

// ---------------------------------------------------------------------------
// RenderHero -- transparent layout, paint-time rect capture, setHidden().
// Mirrors test_focus_manager.cpp's RenderFocus/computeGlobalRect() test
// pattern: a standalone RenderBox, laid out and painted directly.
// ---------------------------------------------------------------------------

TEST(RenderHero, TransparentLayoutSizesToChild)
{
    cw::RenderHero box;
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 42.0f;
    child->height = 24.0f;
    box.setChild(child);

    box.layout(cw::BoxConstraints::loose({200.0f, 200.0f}));
    EXPECT_FLOAT_EQ(box.size().width, 42.0f);
    EXPECT_FLOAT_EQ(box.size().height, 24.0f);
}

TEST(RenderHero, PerformPaintCapturesGlobalRect)
{
    cw::RenderHero box;
    box.layout(cw::BoxConstraints::tight(30.0f, 20.0f));

    cw::PaintContext ctx(800.0f, 800.0f);
    box.paint(ctx, cw::Offset{15.0f, 40.0f});

    const cw::Rect r = box.globalRect();
    EXPECT_FLOAT_EQ(r.x, 15.0f);
    EXPECT_FLOAT_EQ(r.y, 40.0f);
    EXPECT_FLOAT_EQ(r.width, 30.0f);
    EXPECT_FLOAT_EQ(r.height, 20.0f);
}

TEST(RenderHero, SetHiddenSkipsChildPaintButStillCapturesRectAndLayout)
{
    cw::RenderHero box;
    auto child = std::make_shared<cw::RenderColoredBox>();
    child->color = cw::Color::red();
    box.setChild(child);

    box.layout(cw::BoxConstraints::tight(10.0f, 10.0f));
    EXPECT_FALSE(box.hidden());

    box.setHidden(true);
    EXPECT_TRUE(box.hidden());

    cw::PaintContext ctx(800.0f, 800.0f);
    box.paint(ctx, cw::Offset{5.0f, 6.0f});

    // Rect capture and layout are unaffected by hidden -- only the child's
    // own paint is skipped (verified indirectly: no crash / no draw
    // command assertions here, since this test has no draw-list inspection
    // seam -- globalRect() still being correct is the observable proxy).
    const cw::Rect r = box.globalRect();
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 6.0f);
    EXPECT_EQ(box.size().width, 10.0f);
    EXPECT_EQ(box.size().height, 10.0f);

    box.setHidden(false);
    EXPECT_FALSE(box.hidden());
}

// ---------------------------------------------------------------------------
// 7-10. HeroController -- Stage 4: tag-matched manifest across a route
// transition, isolated per route (no rect capture yet -- see Stage 5).
// ---------------------------------------------------------------------------

namespace
{
    // Captures the live NavigatorState* the first time this widget builds --
    // mirrors test_navigator.cpp's own CaptureNavigatorState idiom, but
    // distinctly named (different translation unit, but Unity Build can
    // still merge same-named file-scope symbols into one TU) and also wraps
    // a `child` so the captured route's own content can include a Hero.
    class HeroFlightCaptureState : public cw::StatelessWidget
    {
    public:
        cw::NavigatorState** out = nullptr;
        cw::WidgetRef         child;

        cw::WidgetRef build(cw::BuildContext& ctx) const override
        {
            if (out) *out = cw::Navigator::of(ctx);
            return child;
        }
    };

    // A non-opaque test route -- lets two routes be built simultaneously, so
    // elementForRoute() can return distinct, live elements for both at once.
    // (An opaque top route hides the route beneath it entirely -- see the
    // "opaque push" test below, which exercises that case deliberately.)
    class HeroTransparentTestRoute : public cw::Route
    {
    public:
        explicit HeroTransparentTestRoute(std::function<cw::WidgetRef(cw::BuildContext&)> builder)
            : builder_(std::move(builder)) {}

        cw::WidgetRef build(cw::BuildContext& ctx) override { return builder_(ctx); }
        bool opaque() const override { return false; }

    private:
        std::function<cw::WidgetRef(cw::BuildContext&)> builder_;
    };

    cw::WidgetRef makeTaggedHero(const std::string& tag, float size)
    {
        auto hero   = std::make_shared<cw::Hero>();
        hero->tag   = tag;
        hero->child = std::make_shared<cw::SizedBox>(size, size);
        return hero;
    }
} // namespace

TEST(HeroController, DidChangeTopBuildsIsolatedMatchedPairManifestOnPush)
{
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("shared", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    // Must be set before mount: NavigatorState::initState() wires
    // NavigatorObserver::navigator() only for observers present at that
    // moment (see navigator.cpp), unlike push()/pop()'s direct
    // widget().observers loops, which read the live list on every call.
    nav->observers = {controller};

    auto root = mountHeroTestTree(nav);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(controller->navigator(), nullptr);
    EXPECT_TRUE(controller->manifests().empty()); // no transition has happened yet

    auto route_b = std::make_shared<HeroTransparentTestRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("shared", 30.0f);
    });
    state->push(route_b);

    const auto& manifests = controller->manifests();
    ASSERT_EQ(manifests.size(), 1u);
    EXPECT_EQ(manifests[0].tag, "shared");
    ASSERT_NE(manifests[0].from_element, nullptr);
    ASSERT_NE(manifests[0].to_element, nullptr);

    // The critical correctness check: two same-tag Heroes on different
    // routes must produce two DISTINCT Element*s, not a collided single
    // entry from a non-isolated combined walk across both routes at once.
    EXPECT_NE(manifests[0].from_element, manifests[0].to_element);
    EXPECT_EQ(static_cast<const cw::Hero&>(manifests[0].from_element->widget()).tag, "shared");
    EXPECT_EQ(static_cast<const cw::Hero&>(manifests[0].to_element->widget()).tag, "shared");

    // Stage 5: didChangeTop() with a non-empty manifest schedules a
    // PostFrameCallbacks::schedule() call capturing `controller` (a raw
    // `this`). Drain it here, while `controller` is still alive, so it
    // doesn't sit pending in the global queue past this test's own scope --
    // otherwise a *later* test's runPending() call would fire it against a
    // dangling HeroController*.
    cw::PostFrameCallbacks::runPending();

    root->unmount();
}

TEST(HeroController, DidChangeTopProducesEmptyManifestWhenNoTagsOverlap)
{
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("a", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    nav->observers      = {controller};

    auto root = mountHeroTestTree(nav);
    ASSERT_NE(state, nullptr);

    auto route_b = std::make_shared<HeroTransparentTestRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("b", 30.0f);
    });
    state->push(route_b);

    EXPECT_TRUE(controller->manifests().empty());

    root->unmount();
}

TEST(HeroController, DidChangeTopBuildsFreshReversedManifestOnPop)
{
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("shared", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    nav->observers      = {controller};

    auto root = mountHeroTestTree(nav);
    ASSERT_NE(state, nullptr);

    auto route_b = std::make_shared<HeroTransparentTestRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("shared", 30.0f);
    });
    state->push(route_b);

    ASSERT_EQ(controller->manifests().size(), 1u);
    cw::Element* push_from = controller->manifests()[0].from_element;
    cw::Element* push_to   = controller->manifests()[0].to_element;

    state->pop();

    const auto& pop_manifests = controller->manifests();
    ASSERT_EQ(pop_manifests.size(), 1u);
    EXPECT_EQ(pop_manifests[0].tag, "shared");
    // A fresh manifest, reversed relative to the push: didChangeTop()'s
    // (top_route, previous_top_route) args swap direction on pop(), so
    // from/to must swap too, not just repeat the push's own manifest.
    EXPECT_EQ(pop_manifests[0].from_element, push_to);
    EXPECT_EQ(pop_manifests[0].to_element, push_from);

    // Drain both the push's and the pop's scheduled post-frame callbacks
    // (Stage 5) while `controller` is still alive -- see the matching
    // comment in DidChangeTopBuildsIsolatedMatchedPairManifestOnPush above.
    cw::PostFrameCallbacks::runPending();

    root->unmount();
}

TEST(HeroController, DidChangeTopStaysEmptyWhenPreviousRouteElementIsGone)
{
    // An opaque push (the default) covers the previous route entirely --
    // NavigatorState::build() doesn't build routes below an opaque top at
    // all, so the previous route's element is gone from the tree by the
    // time didChangeTop() fires. HeroController must guard against this
    // (elementForRoute() returning nullptr) rather than crash or misreport.
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("shared", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    nav->observers      = {controller};

    auto root = mountHeroTestTree(nav);
    ASSERT_NE(state, nullptr);

    auto route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("shared", 30.0f);
    });
    EXPECT_NO_THROW(state->push(route_b));

    EXPECT_TRUE(controller->manifests().empty());

    root->unmount();
}

// ---------------------------------------------------------------------------
// 11. HeroController -- Stage 5: the flight itself, driven end-to-end
// through PostFrameCallbacks (no real Renderer needed).
// ---------------------------------------------------------------------------

namespace
{
    // Mounts `root_widget`, finds its root RenderBox, and lays it out --
    // mirrors test_tabs_and_animated_switcher.cpp's own mountAndLayout()
    // idiom (distinctly named per this file's existing Unity Build
    // collision-avoidance convention).
    std::shared_ptr<cw::RenderBox> mountHeroFlightTreeAndLayout(
        cw::WidgetRef root_widget, std::shared_ptr<cw::Element>& out_element, float w, float h)
    {
        out_element = root_widget->createElement();
        out_element->mount(nullptr);

        auto* roe = out_element->findDescendantRenderObjectElement();
        EXPECT_NE(roe, nullptr);
        auto box = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
        EXPECT_NE(box, nullptr);

        box->layout(cw::BoxConstraints::tight(w, h));
        return box;
    }

    // Fixture: installs a live TickerScheduler (so the shared route-
    // transition AnimationController can actually tick) and a live Overlay
    // (so Overlay::insert()/remove() have a real OverlayState::globalState()
    // to act on -- there is no test_draggable.cpp in this codebase to mirror
    // for that setup, so this is built directly from overlay.hpp's own
    // OverlayState::initState()/dispose() contract).
    struct HeroFlightFixture : public ::testing::Test
    {
        cw::TickerScheduler ticker;
        std::shared_ptr<cw::Element> overlay_element;

        void SetUp() override
        {
            cw::TickerScheduler::setActive(&ticker);
            overlay_element = std::make_shared<cw::Overlay>()->createElement();
            overlay_element->mount(nullptr);
        }

        void TearDown() override
        {
            overlay_element->unmount();
            cw::TickerScheduler::setActive(nullptr);
        }

        // Two ticks: AnimationController::onTick() ignores the first (it
        // only establishes the baseline timestamp), so a real advance needs
        // a second tick with a delta -- mirrors test_tabs_and_animated_
        // switcher.cpp's own pumpAnimation() idiom.
        void pumpAnimation(double duration_ms = 300.0)
        {
            ticker.tick(1000);
            ticker.tick(1000 + static_cast<uint64_t>(duration_ms) + 1);
        }
    };
} // namespace

TEST_F(HeroFlightFixture, PushStartsAFlightThatEndsHidingSourceAndRevealingDestination)
{
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("shared", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    nav->observers      = {controller};

    std::shared_ptr<cw::Element> root_element;
    auto root_box = mountHeroFlightTreeAndLayout(nav, root_element, 400.0f, 400.0f);
    cw::PaintContext ctx1(400.0f, 400.0f);
    root_box->paint(ctx1, cw::Offset::zero());
    ASSERT_NE(state, nullptr);

    auto route_b = std::make_shared<HeroTransparentTestRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("shared", 30.0f);
    });
    state->push(route_b); // synchronous rebuild in test environments -- see Element::markNeedsBuild()

    ASSERT_EQ(controller->manifests().size(), 1u);
    cw::Element* push_from = controller->manifests()[0].from_element;
    cw::Element* push_to   = controller->manifests()[0].to_element;
    auto* from_hero = dynamic_cast<cw::RenderHero*>(push_from->nearestRenderObjectElement()->renderObject());
    auto* to_hero   = dynamic_cast<cw::RenderHero*>(push_to->nearestRenderObjectElement()->renderObject());
    ASSERT_NE(from_hero, nullptr);
    ASSERT_NE(to_hero, nullptr);

    // Re-layout/paint so BOTH routes' Heroes (now simultaneously built --
    // route_b is non-opaque) get real global rects captured this "frame",
    // mirroring what Renderer::buildFrame()'s layoutPass()/generateDrawList()
    // would do before its own PostFrameCallbacks::runPending() call.
    root_box->layout(cw::BoxConstraints::tight(400.0f, 400.0f));
    cw::PaintContext ctx2(400.0f, 400.0f);
    root_box->paint(ctx2, cw::Offset::zero());
    const cw::Rect expected_from_rect = from_hero->globalRect();

    // Flight hasn't started yet -- runFlights() only runs via the deferred
    // post-frame callback, not synchronously from didChangeTop()/push().
    EXPECT_TRUE(controller->activeFlights().empty());
    EXPECT_FALSE(from_hero->hidden());
    EXPECT_FALSE(to_hero->hidden());

    cw::PostFrameCallbacks::runPending();

    ASSERT_EQ(controller->activeFlights().size(), 1u);
    const auto& flight = controller->activeFlights()[0];
    EXPECT_EQ(flight.tag, "shared");
    ASSERT_NE(flight.entry, nullptr);
    ASSERT_NE(flight.notifier, nullptr);

    // Both endpoints hidden for the duration of the flight.
    EXPECT_TRUE(from_hero->hidden());
    EXPECT_TRUE(to_hero->hidden());

    // No ticks have advanced the shared animation yet (forward(0.0) leaves
    // it at exactly value 0), so the shuttle starts at the source rect.
    EXPECT_EQ(flight.notifier->value(), expected_from_rect);

    auto* overlay_state = cw::Overlay::globalState();
    ASSERT_NE(overlay_state, nullptr);
    EXPECT_EQ(std::count(overlay_state->entries.begin(), overlay_state->entries.end(), flight.entry), 1);

    pumpAnimation(300.0);

    // Flight complete: shuttle removed from the Overlay, destination revealed.
    EXPECT_EQ(std::count(overlay_state->entries.begin(), overlay_state->entries.end(), flight.entry), 0);
    EXPECT_FALSE(to_hero->hidden());

    root_element->unmount();
}

TEST_F(HeroFlightFixture, PopStartsAReversedFlight)
{
    auto controller = std::make_shared<cw::HeroController>();
    cw::NavigatorState* state = nullptr;

    auto route_a = std::make_shared<cw::PageRoute>([&](cw::BuildContext&) -> cw::WidgetRef {
        auto capture  = std::make_shared<HeroFlightCaptureState>();
        capture->out   = &state;
        capture->child = makeTaggedHero("shared", 10.0f);
        return capture;
    });

    auto nav = std::make_shared<cw::Navigator>();
    nav->initial_route = route_a;
    nav->observers      = {controller};

    std::shared_ptr<cw::Element> root_element;
    auto root_box = mountHeroFlightTreeAndLayout(nav, root_element, 400.0f, 400.0f);
    cw::PaintContext setup_ctx1(400.0f, 400.0f);
    root_box->paint(setup_ctx1, cw::Offset::zero());
    ASSERT_NE(state, nullptr);

    auto route_b = std::make_shared<HeroTransparentTestRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return makeTaggedHero("shared", 30.0f);
    });
    state->push(route_b);
    root_box->layout(cw::BoxConstraints::tight(400.0f, 400.0f));
    cw::PaintContext setup_ctx2(400.0f, 400.0f);
    root_box->paint(setup_ctx2, cw::Offset::zero());
    // Drain the push's own post-frame flight so pop() below starts cleanly.
    cw::PostFrameCallbacks::runPending();
    pumpAnimation(300.0);

    state->pop();
    ASSERT_EQ(controller->manifests().size(), 1u);
    cw::Element* pop_from = controller->manifests()[0].from_element; // route_b's hero
    cw::Element* pop_to   = controller->manifests()[0].to_element;   // route_a's hero
    auto* from_hero = dynamic_cast<cw::RenderHero*>(pop_from->nearestRenderObjectElement()->renderObject());
    auto* to_hero   = dynamic_cast<cw::RenderHero*>(pop_to->nearestRenderObjectElement()->renderObject());
    ASSERT_NE(from_hero, nullptr);
    ASSERT_NE(to_hero, nullptr);

    root_box->layout(cw::BoxConstraints::tight(400.0f, 400.0f));
    cw::PaintContext pop_ctx(400.0f, 400.0f);
    root_box->paint(pop_ctx, cw::Offset::zero());
    const cw::Rect expected_from_rect = from_hero->globalRect();

    cw::PostFrameCallbacks::runPending();

    // active_flights_ also still holds the (already-completed) push flight
    // from earlier in this test -- HeroController never prunes it (see its
    // own doc comment: purely an introspection list). The pop's flight is
    // simply the newest entry.
    ASSERT_FALSE(controller->activeFlights().empty());
    const auto& flight = controller->activeFlights().back();

    // pop()'s shared animation reverses from 1 -> 0 (no ticks advanced it
    // yet), so the tween is oriented so the shuttle still starts at the
    // (popped route's) source rect, not the destination.
    EXPECT_EQ(flight.notifier->value(), expected_from_rect);

    pumpAnimation(300.0);
    EXPECT_FALSE(to_hero->hidden());

    root_element->unmount();
}
