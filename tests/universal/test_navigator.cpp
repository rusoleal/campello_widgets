#include <gtest/gtest.h>
#include <campello_widgets/widgets/navigator.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/ui/animation_controller.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Hero widget, Stage 3 -- NavigatorObserver.
//
// No prior test file exercised Navigator's push()/pop()/pushReplacement() at
// all, so the zero-observer cases here are real, first-time regression
// coverage of that pre-existing behavior, not just a formality.
// ---------------------------------------------------------------------------

namespace
{
    // A NavigatorObserver stand-in recording every call it receives, for
    // asserting exact args/ordering/count.
    struct RecordingObserver : public cw::NavigatorObserver
    {
        struct Call
        {
            std::string kind; // "didPush" | "didPop" | "didReplace" | "didChangeTop"
            cw::Route* route         = nullptr;
            cw::Route* other_route   = nullptr; // previous/old route, or new_top for didPop
            std::shared_ptr<cw::AnimationController> animation;
        };
        std::vector<Call> calls;

        void didPush(cw::Route* route, std::shared_ptr<cw::AnimationController> animation, cw::Route* previous_route) override
        {
            calls.push_back({"didPush", route, previous_route, animation});
        }
        void didPop(cw::Route* route, cw::Route* previous_route) override
        {
            calls.push_back({"didPop", route, previous_route, nullptr});
        }
        void didReplace(cw::Route* new_route, std::shared_ptr<cw::AnimationController> new_animation, cw::Route* old_route) override
        {
            calls.push_back({"didReplace", new_route, old_route, new_animation});
        }
        void didChangeTop(cw::Route* top_route, std::shared_ptr<cw::AnimationController> animation, cw::Route* previous_top_route) override
        {
            calls.push_back({"didChangeTop", top_route, previous_top_route, animation});
        }
    };

    // A widget whose OWN build() -- not the Route's -- captures
    // Navigator::of(ctx). This matters: PageRoute::build(ctx) is called
    // directly inside NavigatorState::build(), with `ctx` being the
    // Navigator's own StatefulElement context -- at that point NavigatorScope
    // (which Navigator::of() looks up) hasn't been constructed or mounted
    // yet (it wraps the route's *returned* content, afterward), so
    // Navigator::of() called with that ctx always returns nullptr. This
    // widget is instead returned *as* the route's content, so its own
    // build() only runs once it mounts as a proper descendant of
    // NavigatorScope's own element -- confirmed by running the tests with
    // the naive "capture from inside the PageRoute lambda" approach first,
    // which failed every case with state == nullptr before this fix.
    class CaptureNavigatorState : public cw::StatelessWidget
    {
    public:
        cw::NavigatorState** out = nullptr;

        cw::WidgetRef build(cw::BuildContext& ctx) const override
        {
            if (out) *out = cw::Navigator::of(ctx);
            return std::make_shared<cw::SizedBox>(10.0f, 10.0f);
        }
    };

    // Mounts `root_widget` as a real Element tree -- same idiom
    // test_hero.cpp's mountHeroTestTree()/test_tabs_and_animated_switcher.cpp's
    // mountAndLayout() already establish (createElement() + mount(nullptr)).
    // Deliberately distinctly-named to avoid a Unity Build redefinition
    // collision with those.
    std::shared_ptr<cw::Element> mountNavigatorTestTree(cw::WidgetRef root_widget)
    {
        auto element = root_widget->createElement();
        element->mount(nullptr);
        return element;
    }

    // Builds a Navigator with a trivial initial route, mounts it, and
    // captures the live NavigatorState* via Navigator::of() from inside the
    // initial route's own build() callback -- the only way to reach a real
    // NavigatorState* from test code, since StatefulElement doesn't expose
    // its State object publicly.
    struct NavigatorFixture
    {
        std::shared_ptr<cw::Route> route_a;
        std::shared_ptr<cw::Route> route_b;
        std::shared_ptr<cw::Route> route_c;
        cw::NavigatorState* state = nullptr;
        std::shared_ptr<cw::Navigator> nav;
        std::shared_ptr<cw::Element> root;

        NavigatorFixture()
        {
            route_a = std::make_shared<cw::PageRoute>([this](cw::BuildContext&) -> cw::WidgetRef {
                auto w = std::make_shared<CaptureNavigatorState>();
                w->out = &state;
                return w;
            });
            nav = std::make_shared<cw::Navigator>();
            nav->initial_route = route_a;
            root = mountNavigatorTestTree(nav);
        }
    };
} // namespace

// ---------------------------------------------------------------------------
// 1. didPush / didChangeTop on push()
// ---------------------------------------------------------------------------

TEST(NavigatorObserver, PushFiresDidPushAndDidChangeTopWithCorrectRoutesAndAnimation)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    auto observer = std::make_shared<RecordingObserver>();
    f.nav->observers = {observer};

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    f.state->push(f.route_b);

    ASSERT_EQ(observer->calls.size(), 2u);
    EXPECT_EQ(observer->calls[0].kind, "didPush");
    EXPECT_EQ(observer->calls[0].route, f.route_b.get());
    EXPECT_EQ(observer->calls[0].other_route, f.route_a.get());
    ASSERT_NE(observer->calls[0].animation, nullptr);

    EXPECT_EQ(observer->calls[1].kind, "didChangeTop");
    EXPECT_EQ(observer->calls[1].route, f.route_b.get());
    EXPECT_EQ(observer->calls[1].other_route, f.route_a.get());
    EXPECT_EQ(observer->calls[1].animation, observer->calls[0].animation);
}

// ---------------------------------------------------------------------------
// 2. didPop / didChangeTop on pop() -- fires before the exit animation
// completes.
// ---------------------------------------------------------------------------

TEST(NavigatorObserver, PopFiresDidPopAndDidChangeTopImmediatelyBeforeAnimationDismissed)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    f.state->push(f.route_b); // no observer yet -- isolate the pop() assertion

    auto observer = std::make_shared<RecordingObserver>();
    f.nav->observers = {observer};

    f.state->pop();

    ASSERT_EQ(observer->calls.size(), 2u);
    EXPECT_EQ(observer->calls[0].kind, "didPop");
    EXPECT_EQ(observer->calls[0].route, f.route_b.get());       // the route being popped
    EXPECT_EQ(observer->calls[0].other_route, f.route_a.get()); // the route that will become top

    EXPECT_EQ(observer->calls[1].kind, "didChangeTop");
    EXPECT_EQ(observer->calls[1].route, f.route_a.get());
    EXPECT_EQ(observer->calls[1].other_route, f.route_b.get());
    ASSERT_NE(observer->calls[1].animation, nullptr);

    // The whole point of this test: notification must fire while the exit
    // animation is still in flight (reversing from 1.0), not after it has
    // settled. reverse() was already called synchronously inside pop(), and
    // no ticker tick has run yet, so status() must not be dismissed here.
    EXPECT_NE(observer->calls[1].animation->status(), cw::AnimationStatus::dismissed);
}

// ---------------------------------------------------------------------------
// 3. didReplace / didChangeTop on pushReplacement()
// ---------------------------------------------------------------------------

TEST(NavigatorObserver, PushReplacementFiresDidReplaceAndDidChangeTop)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    auto observer = std::make_shared<RecordingObserver>();
    f.nav->observers = {observer};

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    f.state->pushReplacement(f.route_b);

    ASSERT_EQ(observer->calls.size(), 2u);
    EXPECT_EQ(observer->calls[0].kind, "didReplace");
    EXPECT_EQ(observer->calls[0].route, f.route_b.get());
    EXPECT_EQ(observer->calls[0].other_route, f.route_a.get());
    ASSERT_NE(observer->calls[0].animation, nullptr);

    EXPECT_EQ(observer->calls[1].kind, "didChangeTop");
    EXPECT_EQ(observer->calls[1].route, f.route_b.get());
    EXPECT_EQ(observer->calls[1].other_route, f.route_a.get());
}

// ---------------------------------------------------------------------------
// 4. Zero observers registered -- real regression coverage of
// push()/pop()/pushReplacement()'s pre-existing behavior, since no prior
// test exercised these methods at all.
// ---------------------------------------------------------------------------

TEST(Navigator, PushWithNoObserversStillPushesAndAnimates)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);
    EXPECT_FALSE(f.state->canPop());

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    EXPECT_NO_THROW(f.state->push(f.route_b));
    EXPECT_TRUE(f.state->canPop());
}

TEST(Navigator, PopWithNoObserversStillReversesAndCanPopUpdatesAfterPrune)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    f.state->push(f.route_b);
    ASSERT_TRUE(f.state->canPop());

    EXPECT_NO_THROW(f.state->pop());
    // The popped entry is only pruned on the next build() once its animation
    // reaches dismissed -- canPop() still reports true immediately after
    // pop() returns, matching pre-Stage-3 behavior exactly (pop() itself
    // never mutated stack_.size() synchronously, even before this stage).
    EXPECT_TRUE(f.state->canPop());
}

TEST(Navigator, PushReplacementWithNoObserversStillReplaces)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    EXPECT_NO_THROW(f.state->pushReplacement(f.route_b));
    // pushReplacement never grows the stack beyond 1 -- still can't pop.
    EXPECT_FALSE(f.state->canPop());
}

// ---------------------------------------------------------------------------
// 5. Multiple observers -- all notified, not just the first.
// ---------------------------------------------------------------------------

TEST(NavigatorObserver, MultipleObserversAllNotifiedForTheSameEvent)
{
    NavigatorFixture f;
    ASSERT_NE(f.state, nullptr);

    auto observer1 = std::make_shared<RecordingObserver>();
    auto observer2 = std::make_shared<RecordingObserver>();
    f.nav->observers = {observer1, observer2};

    f.route_b = std::make_shared<cw::PageRoute>([](cw::BuildContext&) -> cw::WidgetRef {
        return std::make_shared<cw::SizedBox>(20.0f, 20.0f);
    });
    f.state->push(f.route_b);

    EXPECT_EQ(observer1->calls.size(), 2u);
    EXPECT_EQ(observer2->calls.size(), 2u);
    EXPECT_EQ(observer1->calls[0].route, observer2->calls[0].route);
}
