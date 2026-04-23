#include <gtest/gtest.h>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static cw::RenderSizedBox makeBox(std::optional<float> w = {},
                                  std::optional<float> h = {})
{
    cw::RenderSizedBox b;
    b.width  = w;
    b.height = h;
    return b;
}

// ---------------------------------------------------------------------------
// No child — size determined entirely by explicit values / constraints
// ---------------------------------------------------------------------------

TEST(RenderSizedBox, ExplicitSizeWithinLooseConstraints)
{
    auto box = makeBox(100.0f, 50.0f);
    box.layout(cw::BoxConstraints::loose(200.0f, 200.0f));
    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 50.0f);
}

TEST(RenderSizedBox, ExplicitSizeClampsToMaxConstraint)
{
    auto box = makeBox(300.0f, 300.0f);
    box.layout(cw::BoxConstraints::tight(100.0f, 100.0f));
    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 100.0f);
}

TEST(RenderSizedBox, ExplicitSizeClampsToMinConstraint)
{
    auto box = makeBox(10.0f, 10.0f);
    box.layout(cw::BoxConstraints{50.0f, 200.0f, 50.0f, 200.0f});
    EXPECT_FLOAT_EQ(box.size().width,  50.0f);
    EXPECT_FLOAT_EQ(box.size().height, 50.0f);
}

TEST(RenderSizedBox, NoExplicitSizeTakesMaxFromConstraints)
{
    auto box = makeBox();
    box.layout(cw::BoxConstraints::tight(150.0f, 120.0f));
    EXPECT_FLOAT_EQ(box.size().width,  150.0f);
    EXPECT_FLOAT_EQ(box.size().height, 120.0f);
}

TEST(RenderSizedBox, OnlyWidthSetHeightIsZero)
{
    // Matches Flutter: unspecified dimension defaults to 0 when childless.
    auto box = makeBox(80.0f, {});
    box.layout(cw::BoxConstraints::loose(200.0f, 100.0f));
    EXPECT_FLOAT_EQ(box.size().width,  80.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderSizedBox, OnlyHeightSetWidthIsZero)
{
    // Matches Flutter: unspecified dimension defaults to 0 when childless.
    auto box = makeBox({}, 60.0f);
    box.layout(cw::BoxConstraints::loose(200.0f, 100.0f));
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 60.0f);
}

// ---------------------------------------------------------------------------
// With child — box forces tight constraints on child
// ---------------------------------------------------------------------------

TEST(RenderSizedBox, ChildReceivesTightConstraints)
{
    // Child wants 500×500 but the SizedBox constrains it to 100×60.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 500.0f;
    child->height = 500.0f;

    cw::RenderSizedBox box;
    box.width  = 100.0f;
    box.height = 60.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    EXPECT_FLOAT_EQ(box.size().width,    100.0f);
    EXPECT_FLOAT_EQ(box.size().height,   60.0f);
    EXPECT_FLOAT_EQ(child->size().width,  100.0f);
    EXPECT_FLOAT_EQ(child->size().height, 60.0f);
}

TEST(RenderSizedBox, ChildSizeDoesNotAffectBoxSize)
{
    // Child is tiny — box still reports its explicit size.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 5.0f;
    child->height = 5.0f;

    cw::RenderSizedBox box;
    box.width  = 100.0f;
    box.height = 80.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 80.0f);
}
