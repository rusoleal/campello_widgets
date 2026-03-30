#include <gtest/gtest.h>
#include <campello_widgets/ui/render_aspect_ratio.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// No child — box still sizes correctly
// -----------------------------------------------------------------------

TEST(RenderAspectRatio, NoChildSquareRatio)
{
    cw::RenderAspectRatio box;
    box.aspect_ratio = 1.0f;
    box.layout(cw::BoxConstraints::loose(200, 200));
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderAspectRatio, NoChildWideRatioFitsByWidth)
{
    cw::RenderAspectRatio box;
    box.aspect_ratio = 2.0f;   // width = 2 * height
    box.layout(cw::BoxConstraints::loose(400, 300));
    // Fits by width: 400 wide → height = 200.
    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderAspectRatio, NoChildTallRatioFitsByHeight)
{
    cw::RenderAspectRatio box;
    box.aspect_ratio = 0.5f;   // width = 0.5 * height (tall)
    box.layout(cw::BoxConstraints::loose(400, 300));
    // Fitting by width gives height=800 which exceeds 300.
    // So fit by height: 300 high → width = 150.
    EXPECT_FLOAT_EQ(box.size().width,  150.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderAspectRatio, SixteenByNine)
{
    cw::RenderAspectRatio box;
    box.aspect_ratio = 16.0f / 9.0f;
    box.layout(cw::BoxConstraints::loose(320, 240));
    // By width: 320 / (16/9) = 180 ≤ 240, so fits.
    EXPECT_FLOAT_EQ(box.size().width,  320.0f);
    EXPECT_NEAR(box.size().height, 180.0f, 0.01f);
}

TEST(RenderAspectRatio, TightConstraintsClampSize)
{
    cw::RenderAspectRatio box;
    box.aspect_ratio = 1.0f;
    box.layout(cw::BoxConstraints::tight(100, 100));
    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 100.0f);
}

// -----------------------------------------------------------------------
// Child receives tight constraints equal to computed size
// -----------------------------------------------------------------------

TEST(RenderAspectRatio, ChildIsTightConstrainedToAspectSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderAspectRatio box;
    box.aspect_ratio = 2.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(child->size().width,  400.0f);
    EXPECT_FLOAT_EQ(child->size().height, 200.0f);
}
