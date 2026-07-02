#include <gtest/gtest.h>
#include <campello_widgets/ui/rect.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(Rect, InflatePositiveExpandsOnAllSides)
{
    const cw::Rect r = cw::Rect::fromLTWH(10.0f, 20.0f, 30.0f, 40.0f);
    const cw::Rect inflated = r.inflate(5.0f, 8.0f);

    EXPECT_FLOAT_EQ(inflated.left(),   5.0f);
    EXPECT_FLOAT_EQ(inflated.top(),    12.0f);
    EXPECT_FLOAT_EQ(inflated.right(),  45.0f);
    EXPECT_FLOAT_EQ(inflated.bottom(), 68.0f);
}

TEST(Rect, InflateZeroIsNoOp)
{
    const cw::Rect r = cw::Rect::fromLTWH(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_EQ(r.inflate(0.0f, 0.0f), r);
}

TEST(Rect, InflateNegativeShrinks)
{
    const cw::Rect r = cw::Rect::fromLTWH(0.0f, 0.0f, 20.0f, 20.0f);
    const cw::Rect shrunk = r.inflate(-5.0f, -5.0f);

    EXPECT_FLOAT_EQ(shrunk.left(),   5.0f);
    EXPECT_FLOAT_EQ(shrunk.top(),    5.0f);
    EXPECT_FLOAT_EQ(shrunk.right(),  15.0f);
    EXPECT_FLOAT_EQ(shrunk.bottom(), 15.0f);
}
