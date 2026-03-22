#include <gtest/gtest.h>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Size behaviour — no size factors (default Center behaviour)
// ---------------------------------------------------------------------------

// This test documents the known behaviour: without size factors, RenderAlign
// fills the full available space given by the parent constraints. This is what
// caused the Center-in-Column layout bug where Center consumed all remaining
// column height and pushed sibling widgets off-screen.
TEST(RenderAlign, NoSizeFactorsFillsMaxAvailableSpace)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 20.0f;
    child->height = 10.0f;

    cw::RenderAlign align;
    // Default: alignment=center, no width_factor, no height_factor
    align.setChild(child);
    align.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    // Even though the child is tiny, align takes the full available size.
    EXPECT_FLOAT_EQ(align.size().width,  400.0f);
    EXPECT_FLOAT_EQ(align.size().height, 300.0f);
}

TEST(RenderAlign, NoSizeFactorsTightConstraintsUsesMax)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 50.0f;
    child->height = 50.0f;

    cw::RenderAlign align;
    align.setChild(child);
    align.layout(cw::BoxConstraints::tight(200.0f, 150.0f));

    EXPECT_FLOAT_EQ(align.size().width,  200.0f);
    EXPECT_FLOAT_EQ(align.size().height, 150.0f);
}

// ---------------------------------------------------------------------------
// Size behaviour — with size factors (shrink-wrap mode)
// ---------------------------------------------------------------------------

TEST(RenderAlign, HeightFactorOneShrinkWrapsHeight)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 30.0f;

    cw::RenderAlign align;
    align.height_factor = 1.0f;
    align.setChild(child);
    align.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    // Width still fills available space (no width_factor).
    EXPECT_FLOAT_EQ(align.size().width,  400.0f);
    // Height = child.height * 1.0 = 30.
    EXPECT_FLOAT_EQ(align.size().height, 30.0f);
}

TEST(RenderAlign, WidthFactorOneShrinkWrapsWidth)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 30.0f;

    cw::RenderAlign align;
    align.width_factor = 1.0f;
    align.setChild(child);
    align.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(align.size().width,  80.0f);
    EXPECT_FLOAT_EQ(align.size().height, 300.0f); // still fills height
}

TEST(RenderAlign, BothFactorsOneShrinkWrapsBothAxes)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 60.0f;
    child->height = 40.0f;

    cw::RenderAlign align;
    align.width_factor  = 1.0f;
    align.height_factor = 1.0f;
    align.setChild(child);
    align.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(align.size().width,  60.0f);
    EXPECT_FLOAT_EQ(align.size().height, 40.0f);
}

TEST(RenderAlign, SizeFactorScalesChildSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 50.0f;

    cw::RenderAlign align;
    align.width_factor  = 2.0f;
    align.height_factor = 3.0f;
    align.setChild(child);
    align.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(align.size().width,  200.0f); // 100 * 2
    EXPECT_FLOAT_EQ(align.size().height, 150.0f); // 50  * 3
}

// ---------------------------------------------------------------------------
// No child
// ---------------------------------------------------------------------------

TEST(RenderAlign, NoChildFillsMaxAvailableSpace)
{
    cw::RenderAlign align;
    align.layout(cw::BoxConstraints::tight(320.0f, 240.0f));

    EXPECT_FLOAT_EQ(align.size().width,  320.0f);
    EXPECT_FLOAT_EQ(align.size().height, 240.0f);
}
