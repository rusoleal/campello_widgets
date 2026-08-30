#include <gtest/gtest.h>
#include "ui/vertices_geometry.hpp"

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Triangles mode
// -----------------------------------------------------------------------

TEST(VerticesGeometry, TrianglesWithExplicitIndices)
{
    cw::Vertices v;
    v.mode = cw::VertexMode::triangles;
    v.positions = {{0, 0}, {10, 0}, {0, 10}, {10, 10}};
    v.indices   = {0, 1, 2, 1, 3, 2};

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    ASSERT_EQ(built.vertices.size(), 4u);
    ASSERT_EQ(built.indices.size(), 6u);
    EXPECT_EQ(built.indices[0], 0u);
    EXPECT_EQ(built.indices[1], 1u);
    EXPECT_EQ(built.indices[2], 2u);
    EXPECT_EQ(built.indices[3], 1u);
    EXPECT_EQ(built.indices[4], 3u);
    EXPECT_EQ(built.indices[5], 2u);
}

TEST(VerticesGeometry, TrianglesWithoutIndicesUsesImplicitOrder)
{
    cw::Vertices v;
    v.mode = cw::VertexMode::triangles;
    v.positions = {{0, 0}, {10, 0}, {0, 10}};

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    ASSERT_EQ(built.indices.size(), 3u);
    EXPECT_EQ(built.indices[0], 0u);
    EXPECT_EQ(built.indices[1], 1u);
    EXPECT_EQ(built.indices[2], 2u);
}

TEST(VerticesGeometry, TrianglesTruncatesToMultipleOfThree)
{
    cw::Vertices v;
    v.mode = cw::VertexMode::triangles;
    v.positions = {{0, 0}, {10, 0}, {0, 10}, {10, 10}};
    v.indices   = {0, 1, 2, 3}; // 4 indices -- not a multiple of 3

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    EXPECT_EQ(built.indices.size(), 3u); // trailing incomplete triangle dropped
}

// -----------------------------------------------------------------------
// triangleStrip -- winding-correct expansion
// -----------------------------------------------------------------------

TEST(VerticesGeometry, TriangleStripExpandsWithAlternatingWinding)
{
    cw::Vertices v;
    v.mode = cw::VertexMode::triangleStrip;
    v.positions = {{0, 0}, {1, 0}, {2, 0}, {3, 0}}; // 4 verts -> 2 triangles

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    ASSERT_EQ(built.indices.size(), 6u);
    // Triangle 0 (even): (0,1,2)
    EXPECT_EQ(built.indices[0], 0u);
    EXPECT_EQ(built.indices[1], 1u);
    EXPECT_EQ(built.indices[2], 2u);
    // Triangle 1 (odd, k=1): (idx[2],idx[1],idx[3]) = (2,1,3) -- swapped
    // winding order convention keeps front-face orientation consistent.
    EXPECT_EQ(built.indices[3], 2u);
    EXPECT_EQ(built.indices[4], 1u);
    EXPECT_EQ(built.indices[5], 3u);
}

// -----------------------------------------------------------------------
// triangleFan -- every triangle shares index 0
// -----------------------------------------------------------------------

TEST(VerticesGeometry, TriangleFanSharesFirstIndex)
{
    cw::Vertices v;
    v.mode = cw::VertexMode::triangleFan;
    v.positions = {{0, 0}, {1, 0}, {2, 0}, {3, 0}}; // 4 verts -> 2 triangles

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    ASSERT_EQ(built.indices.size(), 6u);
    EXPECT_EQ(built.indices[0], 0u);
    EXPECT_EQ(built.indices[1], 1u);
    EXPECT_EQ(built.indices[2], 2u);
    EXPECT_EQ(built.indices[3], 0u);
    EXPECT_EQ(built.indices[4], 2u);
    EXPECT_EQ(built.indices[5], 3u);
}

// -----------------------------------------------------------------------
// Per-vertex color blending -- reuses blendColors(), mirrors
// test_color_filter.cpp's approach.
// -----------------------------------------------------------------------

TEST(VerticesGeometry, EmptyColorsFallsBackToPaintColorForEveryVertex)
{
    cw::Vertices v;
    v.positions = {{0, 0}, {1, 0}, {2, 0}};
    // v.colors left empty

    const cw::Color paint_color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.8f);
    auto built = cw::buildTriangleListVertices(v, paint_color, cw::BlendMode::srcOver);

    ASSERT_EQ(built.vertices.size(), 3u);
    for (const auto& vert : built.vertices)
    {
        EXPECT_FLOAT_EQ(vert.color.r, paint_color.r);
        EXPECT_FLOAT_EQ(vert.color.g, paint_color.g);
        EXPECT_FLOAT_EQ(vert.color.b, paint_color.b);
        EXPECT_FLOAT_EQ(vert.color.a, paint_color.a);
    }
}

TEST(VerticesGeometry, DstBlendModeKeepsVertexColorsUnchanged)
{
    cw::Vertices v;
    v.positions = {{0, 0}, {1, 0}, {2, 0}};
    v.colors = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),
        cw::Color::fromRGB(0.0f, 0.0f, 1.0f),
    };

    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    ASSERT_EQ(built.vertices.size(), 3u);
    EXPECT_FLOAT_EQ(built.vertices[0].color.r, 1.0f);
    EXPECT_FLOAT_EQ(built.vertices[0].color.g, 0.0f);
    EXPECT_FLOAT_EQ(built.vertices[1].color.g, 1.0f);
    EXPECT_FLOAT_EQ(built.vertices[2].color.b, 1.0f);
}

TEST(VerticesGeometry, SrcInModulatesVertexAlphaByPaintAlpha)
{
    cw::Vertices v;
    v.positions = {{0, 0}, {1, 0}, {2, 0}};
    v.colors = {
        cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f),
        cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f),
        cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f),
    };

    const cw::Color paint_color = cw::Color::fromRGBA(1.0f, 0.0f, 0.0f, 0.5f);
    auto built = cw::buildTriangleListVertices(v, paint_color, cw::BlendMode::srcIn);

    // srcIn: result = src, masked by dst's own alpha -- here dst alpha=1,
    // so result should be exactly paint_color (red, alpha 0.5).
    for (const auto& vert : built.vertices)
    {
        EXPECT_NEAR(vert.color.r, 1.0f, 1e-4f);
        EXPECT_NEAR(vert.color.g, 0.0f, 1e-4f);
        EXPECT_NEAR(vert.color.a, 0.5f, 1e-4f);
    }
}

// -----------------------------------------------------------------------
// Degenerate / empty input
// -----------------------------------------------------------------------

TEST(VerticesGeometry, EmptyPositionsProducesEmptyResult)
{
    cw::Vertices v;
    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    EXPECT_TRUE(built.vertices.empty());
    EXPECT_TRUE(built.indices.empty());
}

TEST(VerticesGeometry, FewerThanThreePositionsProducesNoIndices)
{
    cw::Vertices v;
    v.positions = {{0, 0}, {1, 0}};
    auto built = cw::buildTriangleListVertices(v, cw::Color::white(), cw::BlendMode::dst);
    EXPECT_EQ(built.vertices.size(), 2u);
    EXPECT_TRUE(built.indices.empty());
}
