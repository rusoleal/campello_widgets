#include <gtest/gtest.h>
#include <campello_widgets/ui/render_wrap.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// Helper: create a fixed-size leaf child
static std::shared_ptr<cw::RenderSizedBox> makeLeaf(float w, float h)
{
    auto c = std::make_shared<cw::RenderSizedBox>();
    c->width  = w;
    c->height = h;
    return c;
}

// -----------------------------------------------------------------------
// No children
// -----------------------------------------------------------------------

TEST(RenderWrap, NoChildrenZeroHeight)
{
    cw::RenderWrap wrap;
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().width,  400.0f);
    EXPECT_FLOAT_EQ(wrap.size().height,   0.0f);
}

// -----------------------------------------------------------------------
// Single run — all children fit on one line
// -----------------------------------------------------------------------

TEST(RenderWrap, SingleRunAllFit)
{
    cw::RenderWrap wrap;
    wrap.insertChild(makeLeaf(100, 40), 0);
    wrap.insertChild(makeLeaf(100, 40), 1);
    wrap.insertChild(makeLeaf(100, 40), 2);
    // Total main = 300, fits in 400 wide.
    wrap.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(wrap.size().width,  400.0f);
    EXPECT_FLOAT_EQ(wrap.size().height,  40.0f);
}

// -----------------------------------------------------------------------
// Wraps to second run
// -----------------------------------------------------------------------

TEST(RenderWrap, WrapsToSecondRun)
{
    cw::RenderWrap wrap;
    wrap.insertChild(makeLeaf(150, 30), 0);
    wrap.insertChild(makeLeaf(150, 30), 1);
    wrap.insertChild(makeLeaf(150, 30), 2);
    // Children are 150 each; 3×150 = 450 > 400 → wraps at child 2.
    wrap.layout(cw::BoxConstraints::loose(400, 300));

    // Two runs: run0 = 2×30 = 60h, run1 = 1×30 = 30h → total = 60.
    EXPECT_FLOAT_EQ(wrap.size().height, 60.0f);
}

// -----------------------------------------------------------------------
// Spacing between children on same run
// -----------------------------------------------------------------------

TEST(RenderWrap, SpacingDoesNotCauseUnnecessaryWrap)
{
    cw::RenderWrap wrap;
    wrap.spacing = 10.0f;
    wrap.insertChild(makeLeaf(100, 20), 0);
    wrap.insertChild(makeLeaf(100, 20), 1);
    wrap.insertChild(makeLeaf(100, 20), 2);
    // 3×100 + 2×10 = 320 ≤ 400 → all fit on one run.
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().height, 20.0f);
}

TEST(RenderWrap, SpacingCausesWrap)
{
    cw::RenderWrap wrap;
    wrap.spacing = 20.0f;
    wrap.insertChild(makeLeaf(130, 20), 0);
    wrap.insertChild(makeLeaf(130, 20), 1);
    wrap.insertChild(makeLeaf(130, 20), 2);
    // 3×130 + 2×20 = 430 > 400 → wraps; first run fits 2 (130+20+130=280).
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().height, 40.0f);   // two runs of 20h each
}

// -----------------------------------------------------------------------
// Run spacing
// -----------------------------------------------------------------------

TEST(RenderWrap, RunSpacingBetweenRuns)
{
    cw::RenderWrap wrap;
    wrap.run_spacing = 8.0f;
    wrap.insertChild(makeLeaf(250, 30), 0);
    wrap.insertChild(makeLeaf(250, 30), 1);
    // 2×250=500 > 400 → two runs of one child each.
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    // 30 + 8 + 30 = 68
    EXPECT_FLOAT_EQ(wrap.size().height, 68.0f);
}

// -----------------------------------------------------------------------
// Vertical direction
// -----------------------------------------------------------------------

TEST(RenderWrap, VerticalDirectionWrapsHorizontally)
{
    cw::RenderWrap wrap;
    wrap.direction = cw::Axis::vertical;
    wrap.insertChild(makeLeaf(40, 150), 0);
    wrap.insertChild(makeLeaf(40, 150), 1);
    wrap.insertChild(makeLeaf(40, 150), 2);
    // Main axis = vertical (300 max). 3×150=450 > 300 → wraps.
    // Two runs (2+1), each child 40 wide.
    wrap.layout(cw::BoxConstraints::loose(400, 300));

    // Cross = horizontal. Two runs of 40 wide = 80.
    EXPECT_FLOAT_EQ(wrap.size().width, 80.0f);
}

// -----------------------------------------------------------------------
// Cross-axis alignment: mixed child heights in a run
// -----------------------------------------------------------------------

TEST(RenderWrap, TallestChildDeterminesRunCrossSize)
{
    cw::RenderWrap wrap;
    wrap.insertChild(makeLeaf(100, 20), 0);  // shorter
    wrap.insertChild(makeLeaf(100, 60), 1);  // taller
    wrap.insertChild(makeLeaf(100, 30), 2);  // medium
    // All fit on one run (300 ≤ 400). Run cross = max = 60.
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().height, 60.0f);
}

// -----------------------------------------------------------------------
// clearChildren + re-layout
// -----------------------------------------------------------------------

TEST(RenderWrap, ClearChildrenAndRelayout)
{
    cw::RenderWrap wrap;
    wrap.insertChild(makeLeaf(100, 40), 0);
    wrap.insertChild(makeLeaf(100, 40), 1);
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().height, 40.0f);

    wrap.clearChildren();
    wrap.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(wrap.size().height, 0.0f);
}
