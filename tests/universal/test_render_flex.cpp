#include <gtest/gtest.h>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<cw::RenderSizedBox> makeFixed(float w, float h)
{
    auto b = std::make_shared<cw::RenderSizedBox>();
    b->width  = w;
    b->height = h;
    return b;
}

// A RenderSizedBox with no explicit size — it fills whatever constraints it gets.
static std::shared_ptr<cw::RenderSizedBox> makeFlexible()
{
    return std::make_shared<cw::RenderSizedBox>();
}

// ---------------------------------------------------------------------------
// Column (vertical) — sizes
// ---------------------------------------------------------------------------

TEST(RenderFlex, ColumnTakesMaxHeightWithMainAxisSizeMax)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::max;

    flex.insertChild(makeFixed(100.0f, 40.0f), 0, 0);
    flex.insertChild(makeFixed(100.0f, 60.0f), 1, 0);
    flex.layout(cw::BoxConstraints::tight(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(flex.size().width,  400.0f);
    EXPECT_FLOAT_EQ(flex.size().height, 300.0f); // fills available height
}

TEST(RenderFlex, ColumnTakesNaturalHeightWithMainAxisSizeMin)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::min;

    auto c0 = makeFixed(100.0f, 40.0f);
    auto c1 = makeFixed(100.0f, 60.0f);
    flex.insertChild(c0, 0, 0);
    flex.insertChild(c1, 1, 0);
    flex.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(flex.size().height, 100.0f); // 40 + 60
}

TEST(RenderFlex, ColumnChildrenGetCorrectHeights)
{
    cw::RenderFlex flex;
    flex.axis = cw::Axis::vertical;

    auto c0 = makeFixed(50.0f, 40.0f);
    auto c1 = makeFixed(50.0f, 60.0f);
    flex.insertChild(c0, 0, 0);
    flex.insertChild(c1, 1, 0);
    flex.layout(cw::BoxConstraints::tight(200.0f, 300.0f));

    EXPECT_FLOAT_EQ(c0->size().height, 40.0f);
    EXPECT_FLOAT_EQ(c1->size().height, 60.0f);
}

// ---------------------------------------------------------------------------
// Column — Expanded child
// ---------------------------------------------------------------------------

TEST(RenderFlex, ExpandedChildGetsRemainingHeight)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::max;

    auto fixed    = makeFixed(100.0f, 100.0f);
    auto expanded = makeFlexible(); // no explicit size, receives tight from flex

    flex.insertChild(expanded, 0, 1); // flex=1 (Expanded)
    flex.insertChild(fixed,    1, 0); // flex=0

    flex.layout(cw::BoxConstraints::tight(400.0f, 600.0f));

    EXPECT_FLOAT_EQ(fixed->size().height,    100.0f);
    EXPECT_FLOAT_EQ(expanded->size().height, 500.0f); // 600 - 100
}

TEST(RenderFlex, TwoExpandedChildrenSplitSpaceEvenly)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::max;

    auto e0 = makeFlexible();
    auto e1 = makeFlexible();
    flex.insertChild(e0, 0, 1);
    flex.insertChild(e1, 1, 1);
    flex.layout(cw::BoxConstraints::tight(200.0f, 400.0f));

    EXPECT_FLOAT_EQ(e0->size().height, 200.0f);
    EXPECT_FLOAT_EQ(e1->size().height, 200.0f);
}

TEST(RenderFlex, ExpandedChildWithFixedSiblingsGetsRemainder)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::max;

    auto top    = makeFixed(200.0f, 80.0f);
    auto middle = makeFlexible(); // Expanded
    auto bottom = makeFixed(200.0f, 60.0f);

    flex.insertChild(top,    0, 0);
    flex.insertChild(middle, 1, 1);
    flex.insertChild(bottom, 2, 0);
    flex.layout(cw::BoxConstraints::tight(200.0f, 500.0f));

    EXPECT_FLOAT_EQ(top->size().height,    80.0f);
    EXPECT_FLOAT_EQ(bottom->size().height, 60.0f);
    EXPECT_FLOAT_EQ(middle->size().height, 360.0f); // 500 - 80 - 60
}

// ---------------------------------------------------------------------------
// Column — cross-axis alignment
// ---------------------------------------------------------------------------

TEST(RenderFlex, CrossAxisStretchForcesFullWidth)
{
    cw::RenderFlex flex;
    flex.axis                = cw::Axis::vertical;
    flex.cross_axis_alignment = cw::CrossAxisAlignment::stretch;

    auto child = makeFixed(50.0f, 40.0f); // wants 50 wide
    flex.insertChild(child, 0, 0);
    flex.layout(cw::BoxConstraints::tight(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(child->size().width, 400.0f); // stretched to full width
}

TEST(RenderFlex, CrossAxisStartAllowsChildNaturalWidth)
{
    cw::RenderFlex flex;
    flex.axis                = cw::Axis::vertical;
    flex.cross_axis_alignment = cw::CrossAxisAlignment::start;

    auto child = makeFixed(50.0f, 40.0f);
    flex.insertChild(child, 0, 0);
    flex.layout(cw::BoxConstraints::tight(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(child->size().width, 50.0f); // natural width kept
}

// ---------------------------------------------------------------------------
// Row (horizontal) — mirrored axis behaviour
// ---------------------------------------------------------------------------

TEST(RenderFlex, RowExpandedChildGetsRemainingWidth)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::horizontal;
    flex.main_axis_size = cw::MainAxisSize::max;

    auto fixed    = makeFixed(80.0f, 50.0f);
    auto expanded = makeFlexible();

    flex.insertChild(fixed,    0, 0);
    flex.insertChild(expanded, 1, 1);
    flex.layout(cw::BoxConstraints::tight(400.0f, 200.0f));

    EXPECT_FLOAT_EQ(fixed->size().width,    80.0f);
    EXPECT_FLOAT_EQ(expanded->size().width, 320.0f); // 400 - 80
}

TEST(RenderFlex, RowCrossAxisStretchForcesFullHeight)
{
    cw::RenderFlex flex;
    flex.axis                = cw::Axis::horizontal;
    flex.cross_axis_alignment = cw::CrossAxisAlignment::stretch;

    auto child = makeFixed(60.0f, 30.0f); // wants 30 tall
    flex.insertChild(child, 0, 0);
    flex.layout(cw::BoxConstraints::tight(400.0f, 200.0f));

    EXPECT_FLOAT_EQ(child->size().height, 200.0f); // stretched to full height
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(RenderFlex, NoChildrenResultsInMaxSize)
{
    cw::RenderFlex flex;
    flex.axis           = cw::Axis::vertical;
    flex.main_axis_size = cw::MainAxisSize::max;
    flex.layout(cw::BoxConstraints::tight(200.0f, 300.0f));

    EXPECT_FLOAT_EQ(flex.size().width,  200.0f);
    EXPECT_FLOAT_EQ(flex.size().height, 300.0f);
}

TEST(RenderFlex, ClearChildrenAndRelayout)
{
    cw::RenderFlex flex;
    flex.axis = cw::Axis::vertical;
    flex.insertChild(makeFixed(50.0f, 50.0f), 0, 0);
    flex.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    flex.clearChildren();
    flex.layout(cw::BoxConstraints::tight(200.0f, 200.0f));

    EXPECT_FLOAT_EQ(flex.size().width,  200.0f);
    EXPECT_FLOAT_EQ(flex.size().height, 200.0f);
}
