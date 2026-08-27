#include <gtest/gtest.h>
#include "gpu/stroke_geometry.hpp"
#include <cmath>

namespace cw = systems::leal::campello_widgets;

namespace
{
    float dist(const cw::Offset& a, const cw::Offset& b)
    {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    }
}

// -----------------------------------------------------------------------
// Open 2-point line -- no interior joins, caps only
// -----------------------------------------------------------------------

TEST(StrokeGeometry, TwoPointButtCapProducesOneSegmentNoExtras)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.segments.size(), 1u);
    EXPECT_FLOAT_EQ(g.segments[0].p0.x, 0.0f);
    EXPECT_FLOAT_EQ(g.segments[0].p1.x, 100.0f);
    EXPECT_EQ(g.circles.size(), 0u);
    EXPECT_EQ(g.wedges.size(), 0u);
}

TEST(StrokeGeometry, TwoPointRoundCapProducesTwoCirclesAtEndpoints)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::round, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.circles.size(), 2u);
    EXPECT_FLOAT_EQ(g.circles[0].center.x, 0.0f);
    EXPECT_FLOAT_EQ(g.circles[1].center.x, 100.0f);
    // Segment itself is untouched by round caps (only butt/square affect the
    // segment's own endpoints).
    ASSERT_EQ(g.segments.size(), 1u);
    EXPECT_FLOAT_EQ(g.segments[0].p0.x, 0.0f);
    EXPECT_FLOAT_EQ(g.segments[0].p1.x, 100.0f);
}

TEST(StrokeGeometry, TwoPointSquareCapExtendsSegmentEndpoints)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::square, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.segments.size(), 1u);
    EXPECT_FLOAT_EQ(g.segments[0].p0.x, -5.0f);  // extended half_width=5 backward
    EXPECT_FLOAT_EQ(g.segments[0].p1.x, 105.0f); // extended half_width=5 forward
    EXPECT_EQ(g.circles.size(), 0u); // square cap adds no separate primitive
}

// -----------------------------------------------------------------------
// Open 3-point polyline (90-degree turn) -- one interior join
// -----------------------------------------------------------------------

TEST(StrokeGeometry, ThreePointOpenPolylineHasExactlyOneInteriorJoin)
{
    // Right angle: (0,0) -> (100,0) -> (100,100)
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {100, 100}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::round, 4.0f);

    ASSERT_EQ(g.segments.size(), 2u);
    ASSERT_EQ(g.circles.size(), 1u); // exactly one round join, no caps (butt)
    EXPECT_FLOAT_EQ(g.circles[0].center.x, 100.0f);
    EXPECT_FLOAT_EQ(g.circles[0].center.y, 0.0f);
}

TEST(StrokeGeometry, BevelJoinProducesWedgeAtTurn)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {100, 100}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::bevel, 4.0f);

    ASSERT_EQ(g.wedges.size(), 1u);
    EXPECT_FALSE(g.wedges[0].has_miter_point);
    EXPECT_FLOAT_EQ(g.wedges[0].hub.x, 100.0f);
    EXPECT_FLOAT_EQ(g.wedges[0].hub.y, 0.0f);
}

TEST(StrokeGeometry, MiterJoinAtRightAngleProducesMiterPoint)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {100, 100}};
    const float half_w = 5.0f;
    auto g = cw::buildStrokeGeometry(pts, false, half_w, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.wedges.size(), 1u);
    ASSERT_TRUE(g.wedges[0].has_miter_point);
    // For a 90-degree turn, miter_length = half_width / cos(45 deg) = half_width * sqrt(2).
    const float expected_len = half_w * std::sqrt(2.0f);
    EXPECT_NEAR(dist(g.wedges[0].hub, g.wedges[0].miter_point), expected_len, 1e-3f);
}

TEST(StrokeGeometry, MiterLimitFallsBackToBevelOnSharpTurn)
{
    // A near-reversal (very sharp acute turn) produces an enormous miter
    // length -- must fall back to bevel even though StrokeJoin::miter was
    // requested, given the default miter_limit (4.0).
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {1.0f, 1.0f}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.wedges.size(), 1u);
    EXPECT_FALSE(g.wedges[0].has_miter_point);
}

TEST(StrokeGeometry, StraightThroughVertexProducesDegenerateWedgeNotCrash)
{
    // Collinear points: no real turn. Must not crash or produce NaNs.
    std::vector<cw::Offset> pts{{0, 0}, {50, 0}, {100, 0}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f);

    ASSERT_EQ(g.wedges.size(), 1u);
    EXPECT_TRUE(std::isfinite(g.wedges[0].outer_a.x));
    EXPECT_TRUE(std::isfinite(g.wedges[0].outer_a.y));
}

// -----------------------------------------------------------------------
// Closed polyline (rect outline) -- all vertices are joins, no caps
// -----------------------------------------------------------------------

TEST(StrokeGeometry, ClosedRectHasFourSegmentsFourJoinsNoCapsRegardlessOfCapStyle)
{
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    for (auto cap : {cw::StrokeCap::butt, cw::StrokeCap::round, cw::StrokeCap::square})
    {
        auto g = cw::buildStrokeGeometry(pts, true, 5.0f, cap, cw::StrokeJoin::round, 4.0f);
        EXPECT_EQ(g.segments.size(), 4u);
        EXPECT_EQ(g.circles.size(), 4u); // 4 round joins; round CAP would add more, but there are no open ends
    }
}

TEST(StrokeGeometry, ClosedPolylineToleratesExplicitClosingDuplicate)
{
    // buildPathContours() appends a duplicate closing point when a Path has
    // an explicit close() command -- must be tolerated, not double-counted.
    std::vector<cw::Offset> pts{{0, 0}, {100, 0}, {100, 100}, {0, 100}, {0, 0}};
    auto g = cw::buildStrokeGeometry(pts, true, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::round, 4.0f);
    EXPECT_EQ(g.segments.size(), 4u);
    EXPECT_EQ(g.circles.size(), 4u);
}

// -----------------------------------------------------------------------
// Degenerate input
// -----------------------------------------------------------------------

TEST(StrokeGeometry, FewerThanTwoDistinctPointsProducesEmptyGeometry)
{
    std::vector<cw::Offset> single{{5, 5}};
    auto g1 = cw::buildStrokeGeometry(single, false, 5.0f, cw::StrokeCap::round, cw::StrokeJoin::round, 4.0f);
    EXPECT_EQ(g1.segments.size(), 0u);
    EXPECT_EQ(g1.circles.size(), 0u);

    std::vector<cw::Offset> coincident{{5, 5}, {5, 5}, {5, 5}};
    auto g2 = cw::buildStrokeGeometry(coincident, false, 5.0f, cw::StrokeCap::round, cw::StrokeJoin::round, 4.0f);
    EXPECT_EQ(g2.segments.size(), 0u);
}

TEST(StrokeGeometry, ConsecutiveDuplicatePointsAreDeduped)
{
    std::vector<cw::Offset> pts{{0, 0}, {50, 0}, {50, 0}, {100, 0}};
    auto g = cw::buildStrokeGeometry(pts, false, 5.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f);
    ASSERT_EQ(g.segments.size(), 2u); // not 3 -- the duplicate collapses away
}
