#include <gtest/gtest.h>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// With child
// ---------------------------------------------------------------------------

TEST(RenderPadding, SizeEqualsChildPlusSymmetricInsets)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 50.0f;

    cw::RenderPadding padding;
    padding.padding = cw::EdgeInsets::all(10.0f);
    padding.setChild(child);
    padding.layout(cw::BoxConstraints::loose(300.0f, 300.0f));

    EXPECT_FLOAT_EQ(padding.size().width,  120.0f); // 100 + 20
    EXPECT_FLOAT_EQ(padding.size().height, 70.0f);  // 50  + 20
    EXPECT_FLOAT_EQ(child->size().width,   100.0f);
    EXPECT_FLOAT_EQ(child->size().height,  50.0f);
}

TEST(RenderPadding, SizeEqualsChildPlusAsymmetricInsets)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 50.0f;

    cw::RenderPadding padding;
    // left=5, top=10, right=15, bottom=20 → horizontal=20, vertical=30
    padding.padding = cw::EdgeInsets::only(5.0f, 10.0f, 15.0f, 20.0f);
    padding.setChild(child);
    padding.layout(cw::BoxConstraints::loose(300.0f, 300.0f));

    EXPECT_FLOAT_EQ(padding.size().width,  120.0f); // 100 + 20
    EXPECT_FLOAT_EQ(padding.size().height, 80.0f);  // 50  + 30
}

TEST(RenderPadding, ChildReceivesDeflatedConstraints)
{
    // Child takes max of whatever constraints it gets — no explicit size.
    auto child = std::make_shared<cw::RenderSizedBox>();

    cw::RenderPadding padding;
    padding.padding = cw::EdgeInsets::all(10.0f);
    padding.setChild(child);
    padding.layout(cw::BoxConstraints::tight(200.0f, 100.0f));

    // Child constraints: deflate tight(200,100) by all(10) → tight(180, 80)
    EXPECT_FLOAT_EQ(child->size().width,   180.0f);
    EXPECT_FLOAT_EQ(child->size().height,  80.0f);
    // Padding outer: 180+20=200, 80+20=100
    EXPECT_FLOAT_EQ(padding.size().width,  200.0f);
    EXPECT_FLOAT_EQ(padding.size().height, 100.0f);
}

TEST(RenderPadding, ZeroPaddingPassesThroughChildSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 40.0f;

    cw::RenderPadding padding;
    padding.padding = cw::EdgeInsets::zero();
    padding.setChild(child);
    padding.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    EXPECT_FLOAT_EQ(padding.size().width,  80.0f);
    EXPECT_FLOAT_EQ(padding.size().height, 40.0f);
}

// ---------------------------------------------------------------------------
// Without child
// ---------------------------------------------------------------------------

TEST(RenderPadding, NoChildSizeIsJustInsets)
{
    cw::RenderPadding padding;
    padding.padding = cw::EdgeInsets::all(8.0f);
    padding.layout(cw::BoxConstraints::loose(300.0f, 300.0f));

    EXPECT_FLOAT_EQ(padding.size().width,  16.0f);
    EXPECT_FLOAT_EQ(padding.size().height, 16.0f);
}

TEST(RenderPadding, NoChildConstrainedByParent)
{
    cw::RenderPadding padding;
    padding.padding = cw::EdgeInsets::all(50.0f); // 100 total
    // tight(40,40) < insets total — size should be clamped to tight, not negative
    padding.layout(cw::BoxConstraints::tight(40.0f, 40.0f));

    EXPECT_FLOAT_EQ(padding.size().width,  40.0f);
    EXPECT_FLOAT_EQ(padding.size().height, 40.0f);
}
