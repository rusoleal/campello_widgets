#include <gtest/gtest.h>
#include <campello_widgets/ui/render_intrinsic_width.hpp>
#include <campello_widgets/ui/render_intrinsic_height.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// =========================================================================
// IntrinsicWidth
// =========================================================================

TEST(RenderIntrinsicWidth, NoChildIsZeroWidth)
{
    cw::RenderIntrinsicWidth box;
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width, 0.0f);
}

TEST(RenderIntrinsicWidth, ShrinkWrapsToChildNaturalWidth)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 120.0f;
    child->height = 60.0f;

    cw::RenderIntrinsicWidth box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().width,  120.0f);
    EXPECT_FLOAT_EQ(box.size().height, 60.0f);
}

TEST(RenderIntrinsicWidth, ClampedToParentMaxWidth)
{
    // Child wants 500 wide but parent only allows 200.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 500.0f;
    child->height = 50.0f;

    cw::RenderIntrinsicWidth box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(200, 300));

    EXPECT_FLOAT_EQ(box.size().width, 200.0f);
}

TEST(RenderIntrinsicWidth, StepWidthSnapsUp)
{
    // Child natural width = 110 → snaps up to nearest 50 = 150.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 110.0f;
    child->height = 50.0f;

    cw::RenderIntrinsicWidth box;
    box.step_width = 50.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().width, 150.0f);
}

TEST(RenderIntrinsicWidth, StepWidthExactMultipleUnchanged)
{
    // Child natural width = 100 → snap to 50 = stays 100.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 50.0f;

    cw::RenderIntrinsicWidth box;
    box.step_width = 50.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().width, 100.0f);
}

// =========================================================================
// IntrinsicHeight
// =========================================================================

TEST(RenderIntrinsicHeight, NoChildIsZeroHeight)
{
    cw::RenderIntrinsicHeight box;
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

TEST(RenderIntrinsicHeight, ShrinkWrapsToChildNaturalHeight)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 45.0f;

    cw::RenderIntrinsicHeight box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().width,  80.0f);
    EXPECT_FLOAT_EQ(box.size().height, 45.0f);
}

TEST(RenderIntrinsicHeight, ClampedToParentMaxHeight)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 50.0f;
    child->height = 500.0f;

    cw::RenderIntrinsicHeight box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 200));

    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderIntrinsicHeight, StepHeightSnapsUp)
{
    // Child natural height = 35 → snaps to nearest 20 = 40.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 50.0f;
    child->height = 35.0f;

    cw::RenderIntrinsicHeight box;
    box.step_height = 20.0f;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().height, 40.0f);
}
