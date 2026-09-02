#include <gtest/gtest.h>

#include <campello_widgets/widgets/spacer.hpp>
#include <campello_widgets/widgets/flexible.hpp>
#include <campello_widgets/widgets/visibility.hpp>
#include <campello_widgets/widgets/offstage.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/animated_padding.hpp>
#include <campello_widgets/widgets/default_text_style.hpp>
#include <campello_widgets/widgets/tween_animation_builder.hpp>
#include <campello_widgets/widgets/listener.hpp>
#include <campello_widgets/ui/render_ignore_pointer.hpp>
#include <campello_widgets/ui/render_absorb_pointer.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_listener.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/tween.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Container::build()-style stub -- see test_container.cpp's identical
    // helper. No ancestor InheritedWidgets; only used for build() calls that
    // don't (or shouldn't) find one.
    struct Wave1StubBuildContext : cw::BuildContext
    {
        const cw::Widget& widget() const override { std::abort(); }

    protected:
        const cw::InheritedWidget* getInheritedWidget(const std::type_info&) override
        {
            return nullptr;
        }
    };
}

// ---------------------------------------------------------------------------
// Spacer
// ---------------------------------------------------------------------------

TEST(Spacer, IsDetectedAsFlexibleByDefault)
{
    cw::Spacer s;
    // Flex.cpp's flex-detection is a dynamic_cast<const Flexible*> on the
    // immediate child widget reference -- Spacer must satisfy that directly.
    const auto* f = dynamic_cast<const cw::Flexible*>(&s);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->flex, 1);
}

TEST(Spacer, CustomFlexFactor)
{
    cw::Spacer s(3);
    const auto* f = dynamic_cast<const cw::Flexible*>(&s);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->flex, 3);
}

TEST(Spacer, ChildIsAShrinkSizedBox)
{
    cw::Spacer s;
    auto sb = std::dynamic_pointer_cast<const cw::SizedBox>(s.child);
    ASSERT_NE(sb, nullptr);
    EXPECT_FALSE(sb->width.has_value());
    EXPECT_FALSE(sb->height.has_value());
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

TEST(Visibility, VisibleReturnsChildDirectly)
{
    auto leaf = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    cw::Visibility v(true, leaf);
    Wave1StubBuildContext ctx;
    EXPECT_EQ(v.build(ctx), leaf);
}

TEST(Visibility, InvisibleMaintainStateWrapsInOffstage)
{
    auto leaf = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    cw::Visibility v(false, leaf);
    v.maintain_state = true;
    Wave1StubBuildContext ctx;

    auto off = std::dynamic_pointer_cast<const cw::Offstage>(v.build(ctx));
    ASSERT_NE(off, nullptr);
    EXPECT_TRUE(off->offstage);
    EXPECT_EQ(off->child, leaf);
}

TEST(Visibility, InvisibleWithoutMaintainStateDropsChild)
{
    auto leaf = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    cw::Visibility v(false, leaf);
    v.maintain_state = false;
    Wave1StubBuildContext ctx;

    auto built = v.build(ctx);
    // Default replacement: a shrink SizedBox, not the original child.
    EXPECT_NE(built, leaf);
    auto sb = std::dynamic_pointer_cast<const cw::SizedBox>(built);
    ASSERT_NE(sb, nullptr);
}

TEST(Visibility, InvisibleUsesCustomReplacement)
{
    auto leaf        = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto replacement = std::make_shared<cw::SizedBox>(1.0f, 1.0f);
    cw::Visibility v(false, leaf);
    v.maintain_state = false;
    v.replacement    = replacement;
    Wave1StubBuildContext ctx;

    EXPECT_EQ(v.build(ctx), replacement);
}

// ---------------------------------------------------------------------------
// IgnorePointer / AbsorbPointer
// ---------------------------------------------------------------------------

namespace
{
    // RenderSizedBox never claims hits on its own (hitTestSelf() defaults to
    // false, matching Flutter -- a plain SizedBox is transparent to pointer
    // events). These hit-test tests need a leaf that actually self-claims,
    // so wrap a sized RenderListener (hitTestSelf() -> true) around one.
    std::shared_ptr<cw::RenderListener> makeHittableLeaf(float size)
    {
        auto inner = std::make_shared<cw::RenderSizedBox>();
        inner->width  = size;
        inner->height = size;

        auto leaf = std::make_shared<cw::RenderListener>();
        leaf->setChild(inner);
        return leaf;
    }
}

TEST(RenderIgnorePointer, PassesThroughToChildWhenNotIgnoring)
{
    auto child = makeHittableLeaf(50.0f);

    cw::RenderIgnorePointer box(/*ignoring=*/false);
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 100.0f));

    cw::HitTestResult result;
    EXPECT_TRUE(box.hitTest(result, {25.0f, 25.0f}));
    EXPECT_FALSE(result.isEmpty());
}

TEST(RenderIgnorePointer, SkipsSubtreeWhenIgnoring)
{
    auto child = makeHittableLeaf(50.0f);

    cw::RenderIgnorePointer box(/*ignoring=*/true);
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 100.0f));

    cw::HitTestResult result;
    // Ignoring: neither the child nor the box itself (hitTestSelf stays
    // false, the RenderBox default) claims the hit -- falls through.
    EXPECT_FALSE(box.hitTest(result, {25.0f, 25.0f}));
    EXPECT_TRUE(result.isEmpty());
}

TEST(RenderAbsorbPointer, PassesThroughToChildWhenNotAbsorbing)
{
    auto child = makeHittableLeaf(50.0f);

    cw::RenderAbsorbPointer box(/*absorbing=*/false);
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 100.0f));

    cw::HitTestResult result;
    EXPECT_TRUE(box.hitTest(result, {25.0f, 25.0f}));
    // Deepest-first: the child should be in the path, not just the box.
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(result.path().front().target, static_cast<cw::RenderBox*>(child.get()));
}

TEST(RenderAbsorbPointer, ClaimsHitItselfWhenAbsorbing)
{
    auto child = makeHittableLeaf(50.0f);

    cw::RenderAbsorbPointer box(/*absorbing=*/true);
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 100.0f));

    cw::HitTestResult result;
    // Absorbing: still a hit (the box itself claims it), but the child must
    // NOT be in the path -- that's the whole point vs. IgnorePointer.
    EXPECT_TRUE(box.hitTest(result, {25.0f, 25.0f}));
    ASSERT_FALSE(result.isEmpty());
    for (const auto& entry : result.path())
        EXPECT_NE(entry.target, static_cast<cw::RenderBox*>(child.get()));
}

// ---------------------------------------------------------------------------
// Listener
// ---------------------------------------------------------------------------

TEST(Listener, CreateRenderObjectWiresAllCallbacks)
{
    cw::Listener w;
    bool down_called = false;
    w.on_pointer_down = [&](const cw::PointerEvent&) { down_called = true; };

    auto ro = w.createRenderObject();
    auto* rl = dynamic_cast<cw::RenderListener*>(ro.get());
    ASSERT_NE(rl, nullptr);
    ASSERT_TRUE(static_cast<bool>(rl->on_pointer_down));

    rl->on_pointer_down(cw::PointerEvent{});
    EXPECT_TRUE(down_called);
}

TEST(Listener, UpdateRenderObjectReplacesCallbacks)
{
    cw::Listener w1;
    w1.on_pointer_up = [](const cw::PointerEvent&) {};
    auto ro = w1.createRenderObject();

    cw::Listener w2; // no callbacks set
    w2.updateRenderObject(*ro);

    auto* rl = dynamic_cast<cw::RenderListener*>(ro.get());
    ASSERT_NE(rl, nullptr);
    EXPECT_FALSE(static_cast<bool>(rl->on_pointer_up));
}

// ---------------------------------------------------------------------------
// DefaultTextStyle
// ---------------------------------------------------------------------------

TEST(DefaultTextStyle, OfReturnsDefaultConstructedStyleWithNoAncestor)
{
    Wave1StubBuildContext ctx;
    cw::TextStyle style = cw::DefaultTextStyle::of(ctx);
    EXPECT_EQ(style, cw::TextStyle{});
}

TEST(DefaultTextStyle, UpdateShouldNotifyComparesStyle)
{
    cw::DefaultTextStyle a(cw::TextStyle{}.withFontSize(14.0f), nullptr);
    cw::DefaultTextStyle b(cw::TextStyle{}.withFontSize(14.0f), nullptr);
    cw::DefaultTextStyle c(cw::TextStyle{}.withFontSize(20.0f), nullptr);

    EXPECT_FALSE(a.updateShouldNotify(b));
    EXPECT_TRUE(a.updateShouldNotify(c));
}

// ---------------------------------------------------------------------------
// AnimatedPadding / TweenAnimationBuilder -- createState smoke tests plus a
// direct check of the interpolation math each State drives via lerp<T>
// (see tween.hpp), since a full ticker-driven integration test needs a live
// TickerScheduler this suite doesn't stand up.
// ---------------------------------------------------------------------------

TEST(AnimatedPadding, CreateStateIsNonNull)
{
    cw::AnimatedPadding w(cw::EdgeInsets::all(8.0f));
    auto state = w.createState();
    EXPECT_NE(state, nullptr);
}

TEST(AnimatedPadding, EdgeInsetsLerpMathMatchesExpectedMidpoint)
{
    cw::EdgeInsets from{0.0f, 0.0f, 0.0f, 0.0f};
    cw::EdgeInsets to{20.0f, 10.0f, 20.0f, 10.0f};

    cw::EdgeInsets mid{
        cw::lerp<float>(from.left,   to.left,   0.5),
        cw::lerp<float>(from.top,    to.top,    0.5),
        cw::lerp<float>(from.right,  to.right,  0.5),
        cw::lerp<float>(from.bottom, to.bottom, 0.5),
    };

    EXPECT_FLOAT_EQ(mid.left,   10.0f);
    EXPECT_FLOAT_EQ(mid.top,    5.0f);
    EXPECT_FLOAT_EQ(mid.right,  10.0f);
    EXPECT_FLOAT_EQ(mid.bottom, 5.0f);
}

TEST(TweenAnimationBuilder, CreateStateIsNonNull)
{
    cw::TweenAnimationBuilder<float> w;
    w.value = 10.0f;
    w.builder = [](cw::BuildContext&, const float&, cw::WidgetRef) -> cw::WidgetRef
    {
        return nullptr;
    };
    auto state = w.createState();
    EXPECT_NE(state, nullptr);
}

TEST(TweenAnimationBuilder, FloatTweenEvaluatesLinearly)
{
    cw::Tween<float> t{0.0f, 100.0f};
    EXPECT_FLOAT_EQ(t.evaluate(0.0), 0.0f);
    EXPECT_FLOAT_EQ(t.evaluate(0.5), 50.0f);
    EXPECT_FLOAT_EQ(t.evaluate(1.0), 100.0f);
}

// Hero widget, Stage 1: Tween<Rect>/lerp<Rect> -- interpolates all four
// fields independently, same shape as Tween<Size>'s own width/height lerp.
TEST(Tween, RectTweenEvaluatesEachFieldLinearly)
{
    cw::Tween<cw::Rect> t{
        cw::Rect::fromLTWH(0.0f, 0.0f, 20.0f, 20.0f),
        cw::Rect::fromLTWH(100.0f, 50.0f, 60.0f, 40.0f),
    };

    const cw::Rect at0 = t.evaluate(0.0);
    EXPECT_FLOAT_EQ(at0.x, 0.0f);
    EXPECT_FLOAT_EQ(at0.y, 0.0f);
    EXPECT_FLOAT_EQ(at0.width, 20.0f);
    EXPECT_FLOAT_EQ(at0.height, 20.0f);

    const cw::Rect at1 = t.evaluate(1.0);
    EXPECT_FLOAT_EQ(at1.x, 100.0f);
    EXPECT_FLOAT_EQ(at1.y, 50.0f);
    EXPECT_FLOAT_EQ(at1.width, 60.0f);
    EXPECT_FLOAT_EQ(at1.height, 40.0f);

    const cw::Rect mid = t.evaluate(0.5);
    EXPECT_FLOAT_EQ(mid.x, 50.0f);
    EXPECT_FLOAT_EQ(mid.y, 25.0f);
    EXPECT_FLOAT_EQ(mid.width, 40.0f);
    EXPECT_FLOAT_EQ(mid.height, 30.0f);
}
