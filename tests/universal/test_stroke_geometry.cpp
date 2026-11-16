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

// ---------------------------------------------------------------------------
// buildStrokeAAVertices: a solid core and a fringe straddling the true
// edge, so the stroke keeps its width with soft edges.
// ---------------------------------------------------------------------------

namespace
{
    // Coverage-weighted area of the triangles (alpha linear across each).
    float coveredArea(const std::vector<cw::FillAAVertex>& v)
    {
        float area = 0.0f;
        for (std::size_t i = 0; i + 2 < v.size(); i += 3)
        {
            const auto& a = v[i];
            const auto& b = v[i + 1];
            const auto& c = v[i + 2];
            const float t = std::fabs((b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (c.pos.x - a.pos.x) * (b.pos.y - a.pos.y)) * 0.5f;
            area += t * (a.alpha + b.alpha + c.alpha) / 3.0f;
        }
        return area;
    }
}

TEST(StrokeAAVertices, StraightSegmentKeepsItsWidthWithSoftEdges)
{
    // A 100 x 4 butt-capped segment, a pixel of AA: core 3 wide (alpha 1),
    // a fringe each side from 1.5 to 2.5 off the centre line.
    const auto v = cw::buildStrokeAAVertices({{0.0f, 0.0f}, {100.0f, 0.0f}}, false, 2.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f, 1.0f);
    ASSERT_EQ(v.size() % 3, 0u);
    float maxOff = 0.0f;
    bool soft = false;
    for (const auto& p : v)
    {
        maxOff = std::max(maxOff, std::fabs(p.pos.y));
        if (std::fabs(std::fabs(p.pos.y) - 2.5f) < 1e-4f) { EXPECT_FLOAT_EQ(p.alpha, 0.0f); soft = true; }
        if (std::fabs(std::fabs(p.pos.y) - 1.5f) < 1e-4f) EXPECT_FLOAT_EQ(p.alpha, 1.0f);
    }
    EXPECT_FLOAT_EQ(maxOff, 2.5f);
    EXPECT_TRUE(soft);
    // Coverage integrates to the stroke's own area: 100 x 4.
    EXPECT_NEAR(coveredArea(v), 400.0f, 1e-2f);
}

TEST(StrokeAAVertices, HairlineIsAFainterPixel)
{
    // Half a pixel wide: a pixel-wide core at half coverage, same total ink.
    const auto v = cw::buildStrokeAAVertices({{0.0f, 0.0f}, {10.0f, 0.0f}}, false, 0.25f, cw::StrokeCap::butt, cw::StrokeJoin::miter, 4.0f, 1.0f);
    EXPECT_NEAR(coveredArea(v), 10.0f * 0.5f, 1e-3f);
    for (const auto& p : v) EXPECT_LE(p.alpha, 0.5f + 1e-6f);
}

TEST(StrokeAAVertices, RoundCapsAndJoinsAreSoftDiscs)
{
    const auto v = cw::buildStrokeAAVertices({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}}, false, 2.0f, cw::StrokeCap::round, cw::StrokeJoin::round, 4.0f,
                                             1.0f);
    // A cap's fading ring reaches 2.5 from the start point, at alpha 0.
    bool ring = false;
    for (const auto& p : v)
    {
        const float d = std::hypot(p.pos.x, p.pos.y);
        if (std::fabs(d - 2.5f) < 1e-3f && p.alpha == 0.0f) ring = true;
    }
    EXPECT_TRUE(ring);
}
