#include <gtest/gtest.h>
#include <campello_widgets/ui/render_constrained_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// No child
// -----------------------------------------------------------------------

TEST(RenderConstrainedBox, NoChildUsesZeroSizeWithinEffectiveConstraints)
{
    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints{40, 200, 30, 150};
    box.layout(cw::BoxConstraints::loose(300, 300));
    // No child → constrained zero, but effective min is 40×30.
    EXPECT_FLOAT_EQ(box.size().width,  40.0f);
    EXPECT_FLOAT_EQ(box.size().height, 30.0f);
}

TEST(RenderConstrainedBox, NoChildEmptyAdditional)
{
    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints::loose(300, 300);
    box.layout(cw::BoxConstraints::loose(300, 300));
    EXPECT_FLOAT_EQ(box.size().width,  0.0f);
    EXPECT_FLOAT_EQ(box.size().height, 0.0f);
}

// -----------------------------------------------------------------------
// Additional constraints tighten the available range
// -----------------------------------------------------------------------

TEST(RenderConstrainedBox, ChildReceivesIntersectedMaxConstraints)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 500.0f;
    child->height = 500.0f;

    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints{0, 120, 0, 80};
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 400));

    // Child is clamped to the tighter max (120×80), not the parent max.
    EXPECT_FLOAT_EQ(child->size().width,  120.0f);
    EXPECT_FLOAT_EQ(child->size().height, 80.0f);
}

TEST(RenderConstrainedBox, ParentMaxWinsWhenTighterThanAdditional)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 500.0f;
    child->height = 500.0f;

    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints{0, 1000, 0, 1000};
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(150, 100));

    // Parent is tighter — child clamped to parent max.
    EXPECT_FLOAT_EQ(child->size().width,  150.0f);
    EXPECT_FLOAT_EQ(child->size().height, 100.0f);
}

TEST(RenderConstrainedBox, AdditionalMinEnforcedOnChild)
{
    // Child wants to be tiny, but additional min forces a larger size.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 10.0f;
    child->height = 10.0f;

    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints{50, 200, 40, 200};
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(300, 300));

    EXPECT_FLOAT_EQ(child->size().width,  50.0f);
    EXPECT_FLOAT_EQ(child->size().height, 40.0f);
}

TEST(RenderConstrainedBox, BoxSizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 60.0f;

    cw::RenderConstrainedBox box;
    box.additional_constraints = cw::BoxConstraints{0, 200, 0, 200};
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(300, 300));

    EXPECT_FLOAT_EQ(box.size().width,  80.0f);
    EXPECT_FLOAT_EQ(box.size().height, 60.0f);
}
