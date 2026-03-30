#include <gtest/gtest.h>
#include <campello_widgets/ui/render_fractionally_sized_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// No child — own size is the fraction of available space
// -----------------------------------------------------------------------

TEST(RenderFractionallySizedBox, HalfWidthHalfHeight)
{
    cw::RenderFractionallySizedBox box;
    box.width_factor  = 0.5f;
    box.height_factor = 0.5f;
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 150.0f);
}

TEST(RenderFractionallySizedBox, OnlyWidthFactor)
{
    cw::RenderFractionallySizedBox box;
    box.width_factor = 0.25f;
    // height_factor unset → use full available height
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderFractionallySizedBox, OnlyHeightFactor)
{
    cw::RenderFractionallySizedBox box;
    box.height_factor = 0.75f;
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 225.0f);
}

TEST(RenderFractionallySizedBox, NoFactorsUsesFullSpace)
{
    cw::RenderFractionallySizedBox box;
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderFractionallySizedBox, FactorOneIsFullSize)
{
    cw::RenderFractionallySizedBox box;
    box.width_factor  = 1.0f;
    box.height_factor = 1.0f;
    box.layout(cw::BoxConstraints::loose(200, 150));
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 150.0f);
}

// -----------------------------------------------------------------------
// Child receives tight constraints equal to fractional size
// -----------------------------------------------------------------------

TEST(RenderFractionallySizedBox, ChildIsTightConstrainedToFraction)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderFractionallySizedBox box;
    box.width_factor  = 0.5f;
    box.height_factor = 0.5f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(child->size().width,  200.0f);
    EXPECT_FLOAT_EQ(child->size().height, 150.0f);
}

// -----------------------------------------------------------------------
// Alignment positions child within fractional bounds
// -----------------------------------------------------------------------

TEST(RenderFractionallySizedBox, CenterAlignmentIsDefault)
{
    // Default alignment is center; this just checks it doesn't crash.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 50.0f;
    child->height = 50.0f;

    cw::RenderFractionallySizedBox box;
    box.width_factor  = 0.5f;
    box.height_factor = 0.5f;
    box.alignment     = cw::Alignment::center();
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    // Box size is 200×150; child is tight to 200×150.
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 150.0f);
}
