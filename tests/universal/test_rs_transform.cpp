#include <gtest/gtest.h>
#include <campello_widgets/ui/rs_transform.hpp>
#include <cmath>

namespace cw = systems::leal::campello_widgets;

TEST(RSTransform, NoRotationNoScaleReducesToPlainTranslation)
{
    auto t = cw::RSTransform::fromComponents(
        /*rotation=*/0.0f, /*scale=*/1.0f, /*anchorX=*/0.0f, /*anchorY=*/0.0f,
        /*translateX=*/10.0f, /*translateY=*/20.0f);

    EXPECT_FLOAT_EQ(t.scos, 1.0f);
    EXPECT_FLOAT_EQ(t.ssin, 0.0f);
    EXPECT_FLOAT_EQ(t.tx, 10.0f);
    EXPECT_FLOAT_EQ(t.ty, 20.0f);
}

TEST(RSTransform, ScaleOnlyMultipliesScosScin)
{
    auto t = cw::RSTransform::fromComponents(0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(t.scos, 3.0f);
    EXPECT_FLOAT_EQ(t.ssin, 0.0f);
}

TEST(RSTransform, NinetyDegreeRotation)
{
    const float half_pi = static_cast<float>(M_PI) / 2.0f;
    auto t = cw::RSTransform::fromComponents(half_pi, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(t.scos, 0.0f, 1e-5f);
    EXPECT_NEAR(t.ssin, 1.0f, 1e-5f);
}

TEST(RSTransform, AnchorOffsetChangesTranslationOnlyNotScosScin)
{
    // No rotation/scale -- with an anchor away from the origin, tx/ty must
    // shift by exactly -anchor (since scos=1, ssin=0 -- the anchor point
    // itself must still land at (translateX, translateY)).
    auto t = cw::RSTransform::fromComponents(0.0f, 1.0f, 5.0f, 7.0f, 100.0f, 200.0f);
    EXPECT_FLOAT_EQ(t.scos, 1.0f);
    EXPECT_FLOAT_EQ(t.ssin, 0.0f);
    EXPECT_FLOAT_EQ(t.tx, 100.0f - 5.0f);
    EXPECT_FLOAT_EQ(t.ty, 200.0f - 7.0f);
}

TEST(RSTransform, AnchorPointItselfMapsToTranslation)
{
    // Applying the resulting (scos,ssin,tx,ty) to the anchor point (x=anchorX,
    // y=anchorY) directly must land exactly at (translateX, translateY),
    // regardless of rotation/scale -- this is the defining property of
    // fromComponents()'s anchor handling.
    const float rotation = 0.7f, scale = 2.3f, ax = 4.0f, ay = -3.0f, txp = 50.0f, typ = -10.0f;
    auto t = cw::RSTransform::fromComponents(rotation, scale, ax, ay, txp, typ);

    const float x = t.scos * ax - t.ssin * ay + t.tx;
    const float y = t.ssin * ax + t.scos * ay + t.ty;
    EXPECT_NEAR(x, txp, 1e-4f);
    EXPECT_NEAR(y, typ, 1e-4f);
}
