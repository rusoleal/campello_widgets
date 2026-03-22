#include <gtest/gtest.h>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// BoxConstraints::tight
// ---------------------------------------------------------------------------

TEST(BoxConstraints, TightHasEqualMinMax)
{
    const auto c = cw::BoxConstraints::tight(100.0f, 200.0f);
    EXPECT_FLOAT_EQ(c.min_width,  100.0f);
    EXPECT_FLOAT_EQ(c.max_width,  100.0f);
    EXPECT_FLOAT_EQ(c.min_height, 200.0f);
    EXPECT_FLOAT_EQ(c.max_height, 200.0f);
}

TEST(BoxConstraints, TightFromSize)
{
    const auto c = cw::BoxConstraints::tight(cw::Size{320.0f, 480.0f});
    EXPECT_FLOAT_EQ(c.min_width,  320.0f);
    EXPECT_FLOAT_EQ(c.max_width,  320.0f);
    EXPECT_FLOAT_EQ(c.min_height, 480.0f);
    EXPECT_FLOAT_EQ(c.max_height, 480.0f);
}

// ---------------------------------------------------------------------------
// BoxConstraints::loose
// ---------------------------------------------------------------------------

TEST(BoxConstraints, LooseHasZeroMin)
{
    const auto c = cw::BoxConstraints::loose(300.0f, 400.0f);
    EXPECT_FLOAT_EQ(c.min_width,  0.0f);
    EXPECT_FLOAT_EQ(c.max_width,  300.0f);
    EXPECT_FLOAT_EQ(c.min_height, 0.0f);
    EXPECT_FLOAT_EQ(c.max_height, 400.0f);
}

// ---------------------------------------------------------------------------
// BoxConstraints::constrain
// ---------------------------------------------------------------------------

TEST(BoxConstraints, ConstrainClampsWidthBelowMin)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 50.0f, 100.0f};
    EXPECT_FLOAT_EQ(c.constrain({30.0f, 70.0f}).width, 50.0f);
}

TEST(BoxConstraints, ConstrainKeepsWidthWithinBounds)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 50.0f, 100.0f};
    EXPECT_FLOAT_EQ(c.constrain({80.0f, 80.0f}).width, 80.0f);
}

TEST(BoxConstraints, ConstrainClampsWidthAboveMax)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 50.0f, 100.0f};
    EXPECT_FLOAT_EQ(c.constrain({150.0f, 80.0f}).width, 100.0f);
}

TEST(BoxConstraints, ConstrainClampsHeightBelowMin)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 50.0f, 100.0f};
    EXPECT_FLOAT_EQ(c.constrain({70.0f, 20.0f}).height, 50.0f);
}

TEST(BoxConstraints, ConstrainClampsHeightAboveMax)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 50.0f, 100.0f};
    EXPECT_FLOAT_EQ(c.constrain({70.0f, 200.0f}).height, 100.0f);
}

// ---------------------------------------------------------------------------
// BoxConstraints::deflate
// ---------------------------------------------------------------------------

TEST(BoxConstraints, DeflateReducesBySymmetricInsets)
{
    const auto c = cw::BoxConstraints{0.0f, 200.0f, 0.0f, 300.0f};
    const auto d = c.deflate(cw::EdgeInsets::all(10.0f));
    EXPECT_FLOAT_EQ(d.max_width,  180.0f); // 200 - 20
    EXPECT_FLOAT_EQ(d.max_height, 280.0f); // 300 - 20
    EXPECT_FLOAT_EQ(d.min_width,  0.0f);
    EXPECT_FLOAT_EQ(d.min_height, 0.0f);
}

TEST(BoxConstraints, DeflateReducesByAsymmetricInsets)
{
    const auto c = cw::BoxConstraints{0.0f, 200.0f, 0.0f, 300.0f};
    // left=5, top=10, right=15, bottom=20 → horizontal=20, vertical=30
    const auto d = c.deflate(cw::EdgeInsets::only(5.0f, 10.0f, 15.0f, 20.0f));
    EXPECT_FLOAT_EQ(d.max_width,  180.0f); // 200 - 20
    EXPECT_FLOAT_EQ(d.max_height, 270.0f); // 300 - 30
}

TEST(BoxConstraints, DeflateClampsToZeroWhenInsetsExceedMax)
{
    const auto c = cw::BoxConstraints{0.0f, 10.0f, 0.0f, 10.0f};
    const auto d = c.deflate(cw::EdgeInsets::all(20.0f));
    EXPECT_FLOAT_EQ(d.max_width,  0.0f);
    EXPECT_FLOAT_EQ(d.max_height, 0.0f);
}

TEST(BoxConstraints, DeflateAlsoReducesMin)
{
    const auto c = cw::BoxConstraints{40.0f, 200.0f, 40.0f, 300.0f};
    const auto d = c.deflate(cw::EdgeInsets::all(10.0f));
    EXPECT_FLOAT_EQ(d.min_width,  20.0f); // 40 - 20
    EXPECT_FLOAT_EQ(d.min_height, 20.0f); // 40 - 20
}

// ---------------------------------------------------------------------------
// BoxConstraints::loosen
// ---------------------------------------------------------------------------

TEST(BoxConstraints, LoosenSetsMinToZero)
{
    const auto c = cw::BoxConstraints{50.0f, 100.0f, 60.0f, 120.0f};
    const auto l = c.loosen();
    EXPECT_FLOAT_EQ(l.min_width,  0.0f);
    EXPECT_FLOAT_EQ(l.max_width,  100.0f);
    EXPECT_FLOAT_EQ(l.min_height, 0.0f);
    EXPECT_FLOAT_EQ(l.max_height, 120.0f);
}

TEST(BoxConstraints, LoosenPreservesMax)
{
    const auto c = cw::BoxConstraints::tight(250.0f, 400.0f);
    const auto l = c.loosen();
    EXPECT_FLOAT_EQ(l.max_width,  250.0f);
    EXPECT_FLOAT_EQ(l.max_height, 400.0f);
}

// ---------------------------------------------------------------------------
// BoxConstraints::operator==
// ---------------------------------------------------------------------------

TEST(BoxConstraints, EqualityReturnsTrueForIdentical)
{
    const auto a = cw::BoxConstraints::tight(100.0f, 200.0f);
    const auto b = cw::BoxConstraints::tight(100.0f, 200.0f);
    EXPECT_EQ(a, b);
}

TEST(BoxConstraints, EqualityReturnsFalseWhenDifferent)
{
    const auto a = cw::BoxConstraints::tight(100.0f, 200.0f);
    const auto b = cw::BoxConstraints::tight(100.0f, 201.0f);
    EXPECT_NE(a, b);
}
