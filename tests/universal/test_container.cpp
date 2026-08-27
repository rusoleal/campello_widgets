#include <gtest/gtest.h>
#include <cstdlib>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/clip_rect.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/limited_box.hpp>
#include <campello_widgets/widgets/transform.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Container::build() never reads its BuildContext argument -- this stub
    // only exists to satisfy the signature.
    struct StubBuildContext : cw::BuildContext
    {
        const cw::Widget& widget() const override { std::abort(); }

    protected:
        const cw::InheritedWidget* getInheritedWidget(const std::type_info&) override
        {
            return nullptr;
        }
    };

    // A trivial leaf widget used as Container::child in tests that need a
    // real, identifiable child to find at the bottom of the composed tree.
    class LeafWidget : public cw::SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<cw::RenderObject> createRenderObject() const override
        {
            return std::make_shared<cw::RenderSizedBox>();
        }
        void updateRenderObject(cw::RenderObject&) const override {}
    };

    cw::WidgetRef buildContainer(const cw::Container& c)
    {
        StubBuildContext ctx;
        return c.build(ctx);
    }
}

// -----------------------------------------------------------------------
// Empty Container() -- LimitedBox fallback (Flutter parity)
// -----------------------------------------------------------------------

TEST(Container, EmptyContainerProducesLimitedBoxExpandFiller)
{
    cw::Container c;
    auto built = buildContainer(c);

    auto limited = std::dynamic_pointer_cast<const cw::LimitedBox>(built);
    ASSERT_NE(limited, nullptr);
    EXPECT_FLOAT_EQ(limited->max_width, 0.0f);
    EXPECT_FLOAT_EQ(limited->max_height, 0.0f);

    auto expand = std::dynamic_pointer_cast<const cw::ConstrainedBox>(limited->child);
    ASSERT_NE(expand, nullptr);
    EXPECT_EQ(expand->additional_constraints, cw::BoxConstraints::expand());
    EXPECT_EQ(expand->child, nullptr);
}

TEST(Container, PartiallyTightSizeStillGetsFiller)
{
    // Only width given -- height axis is still "not tight", so the filler
    // must still apply (matches Flutter: partial tightness isn't enough).
    cw::Container c;
    c.width = 100.0f;
    auto built = buildContainer(c);

    // width+height both funnel through the *same* ConstrainedBox step
    // further out, so the LimitedBox filler is still the base child here.
    auto outer = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(outer, nullptr);
    auto limited = std::dynamic_pointer_cast<const cw::LimitedBox>(outer->child);
    EXPECT_NE(limited, nullptr);
}

TEST(Container, FullyTightSizeSkipsFillerEntirely)
{
    // Both width and height given -> effective constraints are tight ->
    // no LimitedBox filler needed; ConstrainedBox alone determines the size.
    cw::Container c;
    c.width  = 100.0f;
    c.height = 50.0f;
    auto built = buildContainer(c);

    auto outer = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->child, nullptr); // no filler, no real child either
    EXPECT_TRUE(outer->additional_constraints.isTight());
}

TEST(Container, ChildPresentNeverGetsFiller)
{
    cw::Container c;
    c.child = std::make_shared<LeafWidget>();
    auto built = buildContainer(c);
    EXPECT_EQ(std::dynamic_pointer_cast<const cw::LimitedBox>(built), nullptr);
    EXPECT_EQ(built, c.child); // nothing else set -> child passes through unchanged
}

// -----------------------------------------------------------------------
// Composition order: alignment -> padding -> color/decoration ->
// foreground_decoration -> constraints -> margin -> transform
// -----------------------------------------------------------------------

TEST(Container, AlignmentWrapsChildDirectly)
{
    cw::Container c;
    c.child     = std::make_shared<LeafWidget>();
    c.alignment = cw::Alignment::topRight();
    auto built  = buildContainer(c);

    auto align = std::dynamic_pointer_cast<const cw::Align>(built);
    ASSERT_NE(align, nullptr);
    EXPECT_EQ(align->alignment, cw::Alignment::topRight());
    EXPECT_EQ(align->child, c.child);
}

TEST(Container, PaddingWrapsAlignment)
{
    cw::Container c;
    c.child     = std::make_shared<LeafWidget>();
    c.alignment = cw::Alignment::center();
    c.padding   = cw::EdgeInsets::all(12.0f);
    auto built  = buildContainer(c);

    auto padding = std::dynamic_pointer_cast<const cw::Padding>(built);
    ASSERT_NE(padding, nullptr);
    EXPECT_EQ(padding->padding, cw::EdgeInsets::all(12.0f));
    EXPECT_NE(std::dynamic_pointer_cast<const cw::Align>(padding->child), nullptr);
}

TEST(Container, ColorWrapsPadding)
{
    cw::Container c;
    c.child   = std::make_shared<LeafWidget>();
    c.padding = cw::EdgeInsets::all(8.0f);
    c.color   = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    auto built = buildContainer(c);

    auto colored = std::dynamic_pointer_cast<const cw::ColoredBox>(built);
    ASSERT_NE(colored, nullptr);
    EXPECT_EQ(colored->color, cw::Color::fromRGB(1.0f, 0.0f, 0.0f));
    EXPECT_NE(std::dynamic_pointer_cast<const cw::Padding>(colored->child), nullptr);
}

TEST(Container, DecorationWrapsPadding)
{
    cw::Container c;
    c.child      = std::make_shared<LeafWidget>();
    c.decoration = cw::BoxDecoration{.color = cw::Color::black()};
    auto built   = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    EXPECT_EQ(decorated->position, cw::DecorationPosition::background);
    EXPECT_EQ(decorated->child, c.child); // no padding/border here
}

TEST(Container, ForegroundDecorationWrapsBackgroundDecoration)
{
    cw::Container c;
    c.child                 = std::make_shared<LeafWidget>();
    c.decoration            = cw::BoxDecoration{.color = cw::Color::black()};
    c.foreground_decoration = cw::BoxDecoration{.color = cw::Color::white()};
    auto built = buildContainer(c);

    auto fg = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(fg, nullptr);
    EXPECT_EQ(fg->position, cw::DecorationPosition::foreground);
    EXPECT_EQ(fg->decoration.color, cw::Color::white());

    auto bg = std::dynamic_pointer_cast<const cw::DecoratedBox>(fg->child);
    ASSERT_NE(bg, nullptr);
    EXPECT_EQ(bg->position, cw::DecorationPosition::background);
    EXPECT_EQ(bg->decoration.color, cw::Color::black());
    EXPECT_EQ(bg->child, c.child);
}

TEST(Container, ForegroundDecorationWorksWithColorToo)
{
    // foreground_decoration is independent of color/decoration -- it must
    // still apply even when the background is a plain `color`.
    cw::Container c;
    c.child                 = std::make_shared<LeafWidget>();
    c.color                 = cw::Color::black();
    c.foreground_decoration = cw::BoxDecoration{.color = cw::Color::white()};
    auto built = buildContainer(c);

    auto fg = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(fg, nullptr);
    EXPECT_EQ(fg->position, cw::DecorationPosition::foreground);
    EXPECT_NE(std::dynamic_pointer_cast<const cw::ColoredBox>(fg->child), nullptr);
}

TEST(Container, ConstraintsWrapForegroundDecoration)
{
    cw::Container c;
    c.child  = std::make_shared<LeafWidget>();
    c.width  = 40.0f;
    c.height = 30.0f;
    auto built = buildContainer(c);

    auto constrained = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(constrained, nullptr);
    EXPECT_EQ(constrained->child, c.child);
}

TEST(Container, MarginWrapsConstraints)
{
    cw::Container c;
    c.child  = std::make_shared<LeafWidget>();
    c.width  = 40.0f;
    c.margin = cw::EdgeInsets::all(6.0f);
    auto built = buildContainer(c);

    auto marginPad = std::dynamic_pointer_cast<const cw::Padding>(built);
    ASSERT_NE(marginPad, nullptr);
    EXPECT_EQ(marginPad->padding, cw::EdgeInsets::all(6.0f));
    EXPECT_NE(std::dynamic_pointer_cast<const cw::ConstrainedBox>(marginPad->child), nullptr);
}

TEST(Container, TransformWrapsMargin)
{
    cw::Container c;
    c.child     = std::make_shared<LeafWidget>();
    c.margin    = cw::EdgeInsets::all(4.0f);
    c.transform = cw::Matrix4::identity();
    auto built  = buildContainer(c);

    auto transform = std::dynamic_pointer_cast<const cw::Transform>(built);
    ASSERT_NE(transform, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<const cw::Padding>(transform->child), nullptr);
}

TEST(Container, TransformDefaultsToTopLeftPivotNotCenter)
{
    // Container's own doc/behavior: no transformAlignment given -> topLeft
    // (zero pivot offset), unlike Transform's own default (center pivot).
    cw::Container c;
    c.child     = std::make_shared<LeafWidget>();
    c.transform = cw::Matrix4::identity();
    auto built  = buildContainer(c);

    auto transform = std::dynamic_pointer_cast<const cw::Transform>(built);
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(transform->alignment, cw::Alignment::topLeft());
}

TEST(Container, TransformAlignmentOverridesDefault)
{
    cw::Container c;
    c.child               = std::make_shared<LeafWidget>();
    c.transform            = cw::Matrix4::identity();
    c.transform_alignment  = cw::Alignment::center();
    auto built = buildContainer(c);

    auto transform = std::dynamic_pointer_cast<const cw::Transform>(built);
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(transform->alignment, cw::Alignment::center());
}

// -----------------------------------------------------------------------
// Border-width auto-padding (Flutter's Container._paddingIncludingDecoration)
// -----------------------------------------------------------------------

TEST(Container, BorderWidthMergedIntoPaddingWhenNoExplicitPadding)
{
    cw::Container c;
    c.child      = std::make_shared<LeafWidget>();
    c.decoration = cw::BoxDecoration{
        .color  = cw::Color::black(),
        .border = cw::BoxBorder::all(cw::Color::white(), 5.0f),
    };
    auto built = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    auto padding = std::dynamic_pointer_cast<const cw::Padding>(decorated->child);
    ASSERT_NE(padding, nullptr);
    EXPECT_EQ(padding->padding, cw::EdgeInsets::all(5.0f));
    EXPECT_EQ(padding->child, c.child);
}

TEST(Container, BorderWidthAddedToExplicitPadding)
{
    cw::Container c;
    c.child      = std::make_shared<LeafWidget>();
    c.padding    = cw::EdgeInsets::all(10.0f);
    c.decoration = cw::BoxDecoration{
        .color  = cw::Color::black(),
        .border = cw::BoxBorder::all(cw::Color::white(), 4.0f),
    };
    auto built = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    auto padding = std::dynamic_pointer_cast<const cw::Padding>(decorated->child);
    ASSERT_NE(padding, nullptr);
    EXPECT_EQ(padding->padding, cw::EdgeInsets::all(14.0f));
}

TEST(Container, NoBorderLeavesPaddingUntouched)
{
    cw::Container c;
    c.child      = std::make_shared<LeafWidget>();
    c.padding    = cw::EdgeInsets::all(10.0f);
    c.decoration = cw::BoxDecoration{.color = cw::Color::black()}; // no border
    auto built   = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    auto padding = std::dynamic_pointer_cast<const cw::Padding>(decorated->child);
    ASSERT_NE(padding, nullptr);
    EXPECT_EQ(padding->padding, cw::EdgeInsets::all(10.0f));
}

// -----------------------------------------------------------------------
// clip_behavior
// -----------------------------------------------------------------------

TEST(Container, ClipBehaviorNoneAddsNoClipWidget)
{
    cw::Container c;
    c.child        = std::make_shared<LeafWidget>();
    c.decoration   = cw::BoxDecoration{.color = cw::Color::black(), .border_radius = 8.0f};
    c.clip_behavior = cw::Clip::none;
    auto built = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    EXPECT_EQ(decorated->child, c.child); // straight through, no clip wrapper
}

TEST(Container, ClipBehaviorWithBorderRadiusUsesClipRRect)
{
    cw::Container c;
    c.child         = std::make_shared<LeafWidget>();
    c.decoration    = cw::BoxDecoration{.color = cw::Color::black(), .border_radius = 12.0f};
    c.clip_behavior = cw::Clip::antiAlias;
    auto built = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    auto clip = std::dynamic_pointer_cast<const cw::ClipRRect>(decorated->child);
    ASSERT_NE(clip, nullptr);
    EXPECT_FLOAT_EQ(clip->border_radius, 12.0f);
    EXPECT_EQ(clip->child, c.child);
}

TEST(Container, ClipBehaviorWithZeroRadiusUsesClipRect)
{
    cw::Container c;
    c.child         = std::make_shared<LeafWidget>();
    c.decoration    = cw::BoxDecoration{.color = cw::Color::black()}; // border_radius = 0
    c.clip_behavior = cw::Clip::hardEdge;
    auto built = buildContainer(c);

    auto decorated = std::dynamic_pointer_cast<const cw::DecoratedBox>(built);
    ASSERT_NE(decorated, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<const cw::ClipRect>(decorated->child), nullptr);
}

TEST(Container, ClipBehaviorWorksWithColorAlone)
{
    // Flutter parity: clipBehavior doesn't require `decoration` -- `color`
    // alone (which becomes part of Flutter's own internal decoration) is
    // enough. Plain color has no radius, so it's always a rectangular clip.
    cw::Container c;
    c.child         = std::make_shared<LeafWidget>();
    c.color         = cw::Color::black();
    c.clip_behavior = cw::Clip::hardEdge;
    auto built = buildContainer(c);

    auto colored = std::dynamic_pointer_cast<const cw::ColoredBox>(built);
    ASSERT_NE(colored, nullptr);
    auto clip = std::dynamic_pointer_cast<const cw::ClipRect>(colored->child);
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->child, c.child);
}

// -----------------------------------------------------------------------
// effectiveConstraints() -- BoxConstraints.tighten() clamping parity
// -----------------------------------------------------------------------

TEST(Container, WidthAloneProducesTightForConstraints)
{
    cw::Container c;
    c.child = std::make_shared<LeafWidget>();
    c.width = 120.0f;
    auto built = buildContainer(c);

    auto constrained = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(constrained, nullptr);
    EXPECT_EQ(constrained->additional_constraints, cw::BoxConstraints::tightFor(120.0f, std::nullopt));
}

TEST(Container, WidthWithinExistingConstraintsTightensExactly)
{
    cw::Container c;
    c.child       = std::make_shared<LeafWidget>();
    c.width       = 80.0f;
    c.constraints = cw::BoxConstraints{50.0f, 100.0f, 0.0f, 200.0f};
    auto built    = buildContainer(c);

    auto constrained = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(constrained, nullptr);
    const auto& eff = constrained->additional_constraints;
    EXPECT_FLOAT_EQ(eff.min_width, 80.0f);
    EXPECT_FLOAT_EQ(eff.max_width, 80.0f);
    EXPECT_FLOAT_EQ(eff.min_height, 0.0f);   // height untouched (not given)
    EXPECT_FLOAT_EQ(eff.max_height, 200.0f);
}

TEST(Container, WidthAboveConstraintsMaxClampsInsteadOfWidening)
{
    // Regression test: width=200 outside constraints' own [50,100] range
    // must clamp to 100 (the caller's own max), not force min=max=200.
    cw::Container c;
    c.child       = std::make_shared<LeafWidget>();
    c.width       = 200.0f;
    c.constraints = cw::BoxConstraints{50.0f, 100.0f, 0.0f, 200.0f};
    auto built    = buildContainer(c);

    auto constrained = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(constrained, nullptr);
    const auto& eff = constrained->additional_constraints;
    EXPECT_FLOAT_EQ(eff.min_width, 100.0f);
    EXPECT_FLOAT_EQ(eff.max_width, 100.0f);
}

TEST(Container, WidthBelowConstraintsMinClampsUpInsteadOfNarrowing)
{
    cw::Container c;
    c.child       = std::make_shared<LeafWidget>();
    c.width       = 10.0f;
    c.constraints = cw::BoxConstraints{50.0f, 100.0f, 0.0f, 200.0f};
    auto built    = buildContainer(c);

    auto constrained = std::dynamic_pointer_cast<const cw::ConstrainedBox>(built);
    ASSERT_NE(constrained, nullptr);
    const auto& eff = constrained->additional_constraints;
    EXPECT_FLOAT_EQ(eff.min_width, 50.0f);
    EXPECT_FLOAT_EQ(eff.max_width, 50.0f);
}

TEST(Container, NoWidthHeightOrConstraintsProducesNoConstrainedBoxAroundRealChild)
{
    cw::Container c;
    c.child = std::make_shared<LeafWidget>();
    auto built = buildContainer(c);
    EXPECT_EQ(built, c.child); // nothing at all wraps a bare child
}
