#include <gtest/gtest.h>
#include <campello_widgets/ui/render_limited_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_constrained_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// No child
// -----------------------------------------------------------------------

TEST(RenderLimitedBox, NoChildAlwaysCollapsesRegardlessOfBoundedness)
{
    // Matches Flutter's RenderLimitedBox exactly: with no child, it always
    // reports `limitConstraints(constraints).constrain(Size.zero)` -- a
    // *bounded* axis does NOT make it fill on its own. "Fills when bounded,
    // collapses when unbounded" is a property of Container's specific
    // LimitedBox(0, 0, ConstrainedBox(expand())) combination (see
    // FillsBoundedAxisCollapsesUnboundedAxisWithExpandingChild below), not
    // of a bare childless LimitedBox.
    cw::RenderLimitedBox box;
    box.layout(cw::BoxConstraints::loose(300.0f, 200.0f));
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderLimitedBox, NoChildUnboundedAxisCollapsesToMaxWidthParam)
{
    cw::RenderLimitedBox box;
    box.max_width  = 0.0f;
    box.max_height = 0.0f;
    box.layout(cw::BoxConstraints::unconstrained());
    // Both axes unbounded, limited by max_width/max_height (0) -> collapses.
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderLimitedBox, NoChildUnboundedAxisNeverReportsInfiniteSize)
{
    // Regression test: this is the exact scenario that used to make
    // Container{.color=...} with no child/size report an infinite size
    // inside e.g. a Row/Column with no Expanded.
    cw::RenderLimitedBox box;
    box.layout(cw::BoxConstraints::unconstrained());
    EXPECT_TRUE(std::isfinite(box.size().width));
    EXPECT_TRUE(std::isfinite(box.size().height));
}

TEST(RenderLimitedBox, NoChildMixedBoundedAndUnboundedAxesStillCollapses)
{
    cw::BoxConstraints mixed{0.0f, 150.0f, 0.0f, std::numeric_limits<float>::infinity()};
    cw::RenderLimitedBox box;
    box.max_height = 40.0f;
    box.layout(mixed);
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

// -----------------------------------------------------------------------
// The actual "fill when bounded, collapse when unbounded" mechanism: a
// LimitedBox(0, 0) wrapping an expanding child (min=max=infinity), exactly
// as Container's build() composes it for its own empty-box fallback.
// -----------------------------------------------------------------------

TEST(RenderLimitedBox, FillsBoundedAxisCollapsesUnboundedAxisWithExpandingChild)
{
    auto expand = std::make_shared<cw::RenderConstrainedBox>();
    expand->additional_constraints = cw::BoxConstraints::expand();

    cw::RenderLimitedBox box;
    box.max_width  = 0.0f;
    box.max_height = 0.0f;
    box.setChild(expand);

    // Bounded parent: fills to the bounded max, same as Container{.color=...}
    // used as a flexible-fill colored box.
    box.layout(cw::BoxConstraints::loose(300.0f, 200.0f));
    EXPECT_FLOAT_EQ(box.size().width,  300.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);

    // Unbounded parent (e.g. inside a Row/Column with no Expanded): collapses
    // to zero instead of reporting an infinite size.
    box.layout(cw::BoxConstraints::unconstrained());
    EXPECT_TRUE(std::isfinite(box.size().width));
    EXPECT_TRUE(std::isfinite(box.size().height));
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderLimitedBox, NoChildMinConstraintWinsOverZeroFallback)
{
    // Unbounded axis normally collapses towards 0, but a nonzero min must
    // still be respected (constrain() clamps up to min).
    cw::BoxConstraints c{20.0f, std::numeric_limits<float>::infinity(),
                          15.0f, std::numeric_limits<float>::infinity()};
    cw::RenderLimitedBox box;
    box.max_width  = 0.0f;
    box.max_height = 0.0f;
    box.layout(c);
    EXPECT_FLOAT_EQ(box.size().width,  20.0f);
    EXPECT_FLOAT_EQ(box.size().height, 15.0f);
}

// -----------------------------------------------------------------------
// With child
// -----------------------------------------------------------------------

TEST(RenderLimitedBox, BoundedAxisPassesThroughUnchangedToChild)
{
    // On a bounded axis the child should see the SAME max as the incoming
    // constraints -- a large child still gets clamped to that bounded max.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderLimitedBox box;
    box.max_width  = 10.0f; // irrelevant: this axis is already bounded
    box.max_height = 10.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(120.0f, 80.0f));

    EXPECT_FLOAT_EQ(child->size().width,  120.0f);
    EXPECT_FLOAT_EQ(child->size().height, 80.0f);
}

TEST(RenderLimitedBox, UnboundedAxisLimitsChildToMaxWidthParam)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderLimitedBox box;
    box.max_width  = 50.0f;
    box.max_height = 30.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::unconstrained());

    EXPECT_FLOAT_EQ(child->size().width,  50.0f);
    EXPECT_FLOAT_EQ(child->size().height, 30.0f);
}

TEST(RenderLimitedBox, BoxSizeMatchesChildOnMixedBoundedUnboundedAxes)
{
    // Width unbounded (limited to 50), height bounded (200) -- the box's
    // own reported size should match the child's actual size on both axes.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderLimitedBox box;
    box.max_width = 50.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints{0.0f, std::numeric_limits<float>::infinity(), 0.0f, 200.0f});

    EXPECT_FLOAT_EQ(box.size().width,  50.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderLimitedBox, DefaultParamsAreNoOpOnUnboundedAxis)
{
    // Default max_width/max_height are infinity, i.e. "no limit" -- matches
    // Flutter's LimitedBox() with no args being a pure passthrough.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 42.0f;
    child->height = 24.0f;

    cw::RenderLimitedBox box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::unconstrained());

    EXPECT_FLOAT_EQ(child->size().width,  42.0f);
    EXPECT_FLOAT_EQ(child->size().height, 24.0f);
}
