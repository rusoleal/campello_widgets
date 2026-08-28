#include <gtest/gtest.h>
#include "gpu/path_fill_aa.hpp"
#include <cmath>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Degenerate contours
// -----------------------------------------------------------------------

TEST(PathFillAA, FewerThanThreePointsProducesNoGeometry)
{
    EXPECT_TRUE(cw::buildFillAASkirt({}, 1.0f).empty());
    EXPECT_TRUE(cw::buildFillAASkirt({{0, 0}}, 1.0f).empty());
    EXPECT_TRUE(cw::buildFillAASkirt({{0, 0}, {10, 0}}, 1.0f).empty());
}

TEST(PathFillAA, ExplicitClosingDuplicateIsTolerated)
{
    // Same square, once with and once without the trailing closing
    // duplicate buildPathContours() appends -- both must produce identical
    // geometry.
    std::vector<cw::Offset> open{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    std::vector<cw::Offset> closed = open;
    closed.push_back(open.front());

    auto g_open   = cw::buildFillAASkirt(open, 1.0f);
    auto g_closed = cw::buildFillAASkirt(closed, 1.0f);

    ASSERT_EQ(g_open.size(), g_closed.size());
    for (size_t i = 0; i < g_open.size(); ++i)
    {
        EXPECT_FLOAT_EQ(g_open[i].pos.x, g_closed[i].pos.x);
        EXPECT_FLOAT_EQ(g_open[i].pos.y, g_closed[i].pos.y);
        EXPECT_FLOAT_EQ(g_open[i].alpha, g_closed[i].alpha);
    }
}

// -----------------------------------------------------------------------
// Square, CCW-per-signed-area winding (0,0)->(10,0)->(10,10)->(0,10)
// -----------------------------------------------------------------------

TEST(PathFillAA, SquareProducesTwoTrianglesPerEdge)
{
    std::vector<cw::Offset> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    auto skirt = cw::buildFillAASkirt(square, 1.0f);

    // 4 edges * 2 triangles * 3 vertices.
    ASSERT_EQ(skirt.size(), 24u);
}

TEST(PathFillAA, InnerVerticesSitExactlyOnTheContourWithFullAlpha)
{
    std::vector<cw::Offset> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    auto skirt = cw::buildFillAASkirt(square, 1.0f);

    // Every triangle's first two vertices are the edge's inner (alpha=1)
    // endpoints, taken directly from the input contour -- see
    // buildFillAASkirt()'s per-edge quad layout.
    for (size_t tri = 0; tri < skirt.size() / 3; ++tri)
    {
        const auto& v0 = skirt[tri * 3 + 0];
        if (v0.alpha == 1.0f)
        {
            bool matches_input = false;
            for (const auto& p : square)
                if (std::abs(v0.pos.x - p.x) < 1e-4f && std::abs(v0.pos.y - p.y) < 1e-4f)
                    matches_input = true;
            EXPECT_TRUE(matches_input);
        }
    }
}

TEST(PathFillAA, NinetyDegreeCornerExtrudesBySqrt2TimesAAWidth)
{
    // Exact analog of the stroke-geometry miter test: at a 90-degree
    // corner, half-angle = 45 degrees, so the bisector extrusion length is
    // aa_width / cos(45deg) = aa_width * sqrt(2) -- see
    // buildFillAASkirt()'s doc comment on reusing the miter-length formula.
    std::vector<cw::Offset> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    auto skirt = cw::buildFillAASkirt(square, 1.0f);

    // Find an outer (alpha=0) vertex coincident with the extrusion at
    // corner (0,0) -- for this winding (signed_area > 0), the outward
    // direction at that corner is up-and-left, i.e. (-1, -1) for
    // aa_width_local = 1.0 (see the plan's hand-derivation).
    bool found = false;
    for (const auto& v : skirt)
    {
        if (v.alpha == 0.0f &&
            std::abs(v.pos.x - (-1.0f)) < 1e-3f &&
            std::abs(v.pos.y - (-1.0f)) < 1e-3f)
        {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PathFillAA, OuterVerticesAreFartherFromCentroidThanInnerOnes)
{
    // Winding-agnostic sanity check: regardless of which way `side` ends up
    // pointing, every outer (alpha=0) vertex must be farther from the
    // polygon's centroid than the corresponding inner (alpha=1) vertex --
    // i.e. the skirt always extrudes outward, never inward.
    std::vector<cw::Offset> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    const cw::Offset centroid{5.0f, 5.0f};
    auto skirt = cw::buildFillAASkirt(square, 1.0f);

    auto distSq = [&](const cw::Offset& p) {
        const float dx = p.x - centroid.x, dy = p.y - centroid.y;
        return dx * dx + dy * dy;
    };

    for (size_t tri = 0; tri < skirt.size() / 3; ++tri)
    {
        // Each triangle mixes alpha=1 and alpha=0 vertices (see the
        // per-edge quad layout); compare any inner/outer pair within it.
        const auto& a = skirt[tri * 3 + 0];
        const auto& c = skirt[tri * 3 + 2];
        if (a.alpha == 1.0f && c.alpha == 0.0f)
            EXPECT_GT(distSq(c.pos), distSq(a.pos));
    }
}

TEST(PathFillAA, ExtrusionDirectionIsWindingInvariant)
{
    // Same physical square, opposite traversal order (signed_area flips
    // sign, flipping `side` to compensate) -- the outward direction at
    // (0,0) must land on the *same* physical point either way, since
    // reversing point order doesn't change the shape or its interior.
    std::vector<cw::Offset> reversed{{0, 0}, {0, 10}, {10, 10}, {10, 0}};
    auto skirt = cw::buildFillAASkirt(reversed, 1.0f);

    bool found = false;
    for (const auto& v : skirt)
    {
        if (v.alpha == 0.0f &&
            std::abs(v.pos.x - (-1.0f)) < 1e-3f &&
            std::abs(v.pos.y - (-1.0f)) < 1e-3f)
        {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
