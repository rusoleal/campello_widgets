#include <gtest/gtest.h>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <cmath>
#include <limits>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Layout — no child
// -----------------------------------------------------------------------

TEST(RenderColoredBox, NoChildBoundedFillsToMax)
{
    cw::RenderColoredBox box;
    box.color = cw::Color::fromRGB(0.2f, 0.5f, 0.9f);
    box.layout(cw::BoxConstraints::loose(400.0f, 300.0f));
    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderColoredBox, NoChildUnboundedAxisNeverReportsInfiniteSize)
{
    // Regression test: this is the scenario that used to make
    // Container{.color=...} with no child/size report an infinite size
    // inside e.g. a Row/Column with no Expanded.
    cw::RenderColoredBox box;
    box.color = cw::Color::black();
    box.layout(cw::BoxConstraints::unconstrained());
    EXPECT_TRUE(std::isfinite(box.size().width));
    EXPECT_TRUE(std::isfinite(box.size().height));
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderColoredBox, NoChildMixedBoundedAndUnboundedAxes)
{
    cw::BoxConstraints mixed{0.0f, 220.0f, 0.0f, std::numeric_limits<float>::infinity()};
    cw::RenderColoredBox box;
    box.color = cw::Color::black();
    box.layout(mixed);
    EXPECT_FLOAT_EQ(box.size().width,  220.0f); // bounded axis: fills to max
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);   // unbounded axis: collapses
}

TEST(RenderColoredBox, NoChildUnboundedButNonZeroMinRespectsMin)
{
    cw::BoxConstraints c{30.0f, std::numeric_limits<float>::infinity(),
                          20.0f, std::numeric_limits<float>::infinity()};
    cw::RenderColoredBox box;
    box.color = cw::Color::black();
    box.layout(c);
    EXPECT_FLOAT_EQ(box.size().width,  30.0f);
    EXPECT_FLOAT_EQ(box.size().height, 20.0f);
}

// -----------------------------------------------------------------------
// Layout — with child
// -----------------------------------------------------------------------

TEST(RenderColoredBox, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 120.0f;
    child->height = 80.0f;

    cw::RenderColoredBox box;
    box.color = cw::Color::white();
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(box.size().width,  120.0f);
    EXPECT_FLOAT_EQ(box.size().height, 80.0f);
}

TEST(RenderColoredBox, ChildClampedToParentMax)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderColoredBox box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 75.0f));

    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 75.0f);
}
