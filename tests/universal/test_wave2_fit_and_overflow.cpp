#include <gtest/gtest.h>

#include <campello_widgets/ui/render_fitted_box.hpp>
#include <campello_widgets/ui/render_constraints_transform_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/widgets/overflow_box.hpp>
#include <campello_widgets/widgets/unconstrained_box.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// RenderFittedBox -- fills available space regardless of child's own size,
// and lays the child out unconstrained (natural size), matching RenderAlign.
// ---------------------------------------------------------------------------

TEST(RenderFittedBox, FillsAvailableSpaceRegardlessOfChildSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 20.0f;
    child->height = 10.0f;

    cw::RenderFittedBox box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400.0f, 300.0f));

    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderFittedBox, ChildLaysOutAtItsNaturalSizeNotTheBoxsSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 20.0f;
    child->height = 10.0f;

    cw::RenderFittedBox box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::tight(400.0f, 300.0f));

    // The child must NOT be forced to the box's own tight size -- FittedBox
    // scales the child's *natural* size to fit, it doesn't stretch the
    // child's own layout to match.
    EXPECT_FLOAT_EQ(child->size().width,  20.0f);
    EXPECT_FLOAT_EQ(child->size().height, 10.0f);
}

TEST(RenderFittedBox, NoChildStillFillsConstraints)
{
    cw::RenderFittedBox box;
    box.layout(cw::BoxConstraints::loose(150.0f, 90.0f));
    EXPECT_FLOAT_EQ(box.size().width,  150.0f);
    EXPECT_FLOAT_EQ(box.size().height, 90.0f);
}

// ---------------------------------------------------------------------------
// RenderConstraintsTransformBox -- OverflowBox / UnconstrainedBox
// ---------------------------------------------------------------------------

TEST(RenderConstraintsTransformBox, SelfSizeIgnoresChildOverflow)
{
    // A child far larger than the parent's own constraints allow.
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 500.0f;
    child->height = 500.0f;

    cw::RenderConstraintsTransformBox box;
    box.constraints_transform = [](const cw::BoxConstraints&) { return cw::BoxConstraints::unconstrained(); };
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100.0f, 80.0f));

    // Box sizes itself from ITS OWN constraints only -- unaffected by the
    // child's much larger actual size. This is the whole point vs.
    // RenderConstrainedBox, which sizes to the (clamped) child size.
    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height, 80.0f);

    // The child, laid out unconstrained, keeps its full natural size --
    // overflowing the box, unclipped.
    EXPECT_FLOAT_EQ(child->size().width,  500.0f);
    EXPECT_FLOAT_EQ(child->size().height, 500.0f);
}

TEST(OverflowBox, UnsetBoundsFallBackToOwnConstraints)
{
    auto leaf = std::make_shared<cw::SizedBox>(30.0f, 30.0f);
    cw::OverflowBox w(leaf);
    // No min/max overrides set at all.
    auto ro = w.createRenderObject();
    auto& box = static_cast<cw::RenderConstraintsTransformBox&>(*ro);

    const cw::BoxConstraints parent = cw::BoxConstraints::loose(200.0f, 150.0f);
    const cw::BoxConstraints child_c = box.constraints_transform(parent);

    EXPECT_FLOAT_EQ(child_c.min_width,  parent.min_width);
    EXPECT_FLOAT_EQ(child_c.max_width,  parent.max_width);
    EXPECT_FLOAT_EQ(child_c.min_height, parent.min_height);
    EXPECT_FLOAT_EQ(child_c.max_height, parent.max_height);
}

TEST(OverflowBox, ExplicitMaxWidthOverridesParentBound)
{
    cw::OverflowBox w;
    w.max_width = 999.0f;
    auto ro = w.createRenderObject();
    auto& box = static_cast<cw::RenderConstraintsTransformBox&>(*ro);

    const cw::BoxConstraints parent = cw::BoxConstraints::loose(200.0f, 150.0f);
    const cw::BoxConstraints child_c = box.constraints_transform(parent);

    EXPECT_FLOAT_EQ(child_c.max_width,  999.0f);
    // Unset axes still fall back.
    EXPECT_FLOAT_EQ(child_c.max_height, parent.max_height);
}

TEST(UnconstrainedBox, AlwaysProducesUnconstrained)
{
    cw::UnconstrainedBox w;
    auto ro = w.createRenderObject();
    auto& box = static_cast<cw::RenderConstraintsTransformBox&>(*ro);

    const cw::BoxConstraints parent = cw::BoxConstraints::tight(50.0f, 40.0f);
    const cw::BoxConstraints child_c = box.constraints_transform(parent);

    EXPECT_EQ(child_c, cw::BoxConstraints::unconstrained());
}
