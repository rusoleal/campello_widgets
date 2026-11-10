#include <gtest/gtest.h>
#include "gpu/text_quad.hpp"

#include <cmath>

namespace cw = systems::leal::campello_widgets;
namespace vm = systems::leal::vector_math;

using M = vm::Matrix4<float>;

// -----------------------------------------------------------------------
// Text quads keep the rasterized size and follow the transform's rotation.
// -----------------------------------------------------------------------

TEST(TextQuad, IdentityIsTheAxisAlignedQuad)
{
    const auto q = cw::textQuadCorners(M::identity(), 10.0f, 20.0f, 30.0f, 8.0f, 1.0f);
    EXPECT_FLOAT_EQ(q[0].x, 9.0f);   // top-left, shifted up-left by the padding
    EXPECT_FLOAT_EQ(q[0].y, 19.0f);
    EXPECT_FLOAT_EQ(q[1].x, 39.0f);  // top-right
    EXPECT_FLOAT_EQ(q[1].y, 19.0f);
    EXPECT_FLOAT_EQ(q[2].x, 9.0f);   // bottom-left
    EXPECT_FLOAT_EQ(q[2].y, 27.0f);
    EXPECT_FLOAT_EQ(q[3].x, 39.0f);  // bottom-right
    EXPECT_FLOAT_EQ(q[3].y, 27.0f);
}

TEST(TextQuad, ScaleMovesTheOriginButNotTheSize)
{
    // The texture is already rasterized at the scaled size.
    const auto q = cw::textQuadCorners(M::scale(vm::Vector3<float>(2.0f, 2.0f, 1.0f)), 10.0f, 20.0f, 30.0f, 8.0f, 0.0f);
    EXPECT_FLOAT_EQ(q[0].x, 20.0f);
    EXPECT_FLOAT_EQ(q[0].y, 40.0f);
    EXPECT_FLOAT_EQ(q[3].x - q[0].x, 30.0f);
    EXPECT_FLOAT_EQ(q[3].y - q[0].y, 8.0f);
}

TEST(TextQuad, RotationTurnsTheQuad)
{
    const M rotation = M::rotateZ(1.5707963267948966f);
    const auto q = cw::textQuadCorners(rotation, 0.0f, 0.0f, 30.0f, 8.0f, 0.0f);
    // The width runs along the transformed x axis, the height along y.
    const auto ex = rotation * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
    const auto ey = rotation * vm::Vector4<float>(0.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_NEAR(q[1].x - q[0].x, ex.x() * 30.0f, 1e-4f);
    EXPECT_NEAR(q[1].y - q[0].y, ex.y() * 30.0f, 1e-4f);
    EXPECT_NEAR(q[2].x - q[0].x, ey.x() * 8.0f, 1e-4f);
    EXPECT_NEAR(q[2].y - q[0].y, ey.y() * 8.0f, 1e-4f);
    // A quarter turn: the width is now vertical.
    EXPECT_NEAR(std::fabs(q[1].y - q[0].y), 30.0f, 1e-4f);
    EXPECT_NEAR(q[1].x - q[0].x, 0.0f, 1e-4f);
}
