#include <gtest/gtest.h>
#include "ui/nine_patch_geometry.hpp"

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Symmetric center, no scaling squeeze -- the common 9-patch case.
// -----------------------------------------------------------------------

TEST(NinePatchGeometry, SymmetricCenterProducesNineOrderedPatches)
{
    // 30x30 texture, center is the middle 10x10 (10px margins on all sides).
    const auto patches = cw::computeNinePatchGeometry(
        30.0f, 30.0f,
        cw::Rect::fromLTWH(10, 10, 10, 10),
        cw::Rect::fromLTWH(0, 0, 60, 60));

    ASSERT_EQ(patches.size(), 9u);

    // Row-major order: (0,0) is the top-left corner patch.
    const auto& top_left = patches[0];
    EXPECT_FLOAT_EQ(top_left.src.x, 0.0f);
    EXPECT_FLOAT_EQ(top_left.src.y, 0.0f);
    EXPECT_NEAR(top_left.src.width,  10.0f / 30.0f, 1e-5f);
    EXPECT_NEAR(top_left.src.height, 10.0f / 30.0f, 1e-5f);
    // Corner keeps its unscaled source pixel size (10x10) in dest space.
    EXPECT_FLOAT_EQ(top_left.dst.x, 0.0f);
    EXPECT_FLOAT_EQ(top_left.dst.y, 0.0f);
    EXPECT_FLOAT_EQ(top_left.dst.width, 10.0f);
    EXPECT_FLOAT_EQ(top_left.dst.height, 10.0f);

    const auto& center = patches[4]; // row 1, col 1
    EXPECT_NEAR(center.src.x, 10.0f / 30.0f, 1e-5f);
    EXPECT_NEAR(center.src.y, 10.0f / 30.0f, 1e-5f);
    // The stretchy middle absorbs whatever space is left: 60 - 10 - 10 = 40.
    EXPECT_FLOAT_EQ(center.dst.width, 40.0f);
    EXPECT_FLOAT_EQ(center.dst.height, 40.0f);

    const auto& bottom_right = patches[8];
    EXPECT_FLOAT_EQ(bottom_right.dst.x, 50.0f);
    EXPECT_FLOAT_EQ(bottom_right.dst.y, 50.0f);
    EXPECT_FLOAT_EQ(bottom_right.dst.width, 10.0f);
    EXPECT_FLOAT_EQ(bottom_right.dst.height, 10.0f);
}

// -----------------------------------------------------------------------
// dst_rect smaller than the source's own unstretched corner regions --
// corners must clamp so opposing corners never overlap.
// -----------------------------------------------------------------------

TEST(NinePatchGeometry, CornersClampToHalfDstWhenSqueezed)
{
    // Same 30x30 texture / 10px margins, but dst is only 12x12 -- far
    // smaller than the two 10px corners would need (20px) on their own.
    const auto patches = cw::computeNinePatchGeometry(
        30.0f, 30.0f,
        cw::Rect::fromLTWH(10, 10, 10, 10),
        cw::Rect::fromLTWH(0, 0, 12, 12));

    // Every corner clamps to half of 12 = 6, so left+right (and top+bottom)
    // never overlap; the middle patch degenerates to zero-width and is
    // dropped from the result entirely.
    for (const auto& patch : patches)
    {
        EXPECT_LE(patch.dst.width, 6.0f + 1e-4f);
        EXPECT_LE(patch.dst.height, 6.0f + 1e-4f);
    }

    const auto& top_left = patches[0];
    EXPECT_FLOAT_EQ(top_left.dst.width, 6.0f);
    EXPECT_FLOAT_EQ(top_left.dst.height, 6.0f);

    const auto& bottom_right = patches.back();
    EXPECT_FLOAT_EQ(bottom_right.dst.x, 6.0f);
    EXPECT_FLOAT_EQ(bottom_right.dst.y, 6.0f);
}

// -----------------------------------------------------------------------
// Degenerate patches (center touching an edge) are omitted, not returned
// as empty rects.
// -----------------------------------------------------------------------

TEST(NinePatchGeometry, CenterTouchingLeftEdgeDropsLeftColumn)
{
    // center starts at x=0, so the left column (src width 0) is degenerate.
    const auto patches = cw::computeNinePatchGeometry(
        20.0f, 20.0f,
        cw::Rect::fromLTWH(0, 5, 10, 10),
        cw::Rect::fromLTWH(0, 0, 40, 40));

    // 3 rows x 2 remaining columns (middle, right) = 6 patches.
    ASSERT_EQ(patches.size(), 6u);
    for (const auto& patch : patches)
    {
        EXPECT_GT(patch.src.width, 0.0f);
        EXPECT_GT(patch.dst.width, 0.0f);
    }
}

// -----------------------------------------------------------------------
// Out-of-bounds `center` is clamped to the texture's own bounds rather
// than producing inverted/nonsensical rects.
// -----------------------------------------------------------------------

TEST(NinePatchGeometry, CenterOutsideTextureBoundsIsClamped)
{
    const auto patches = cw::computeNinePatchGeometry(
        10.0f, 10.0f,
        cw::Rect::fromLTWH(-5, -5, 30, 30), // wildly out of bounds
        cw::Rect::fromLTWH(0, 0, 20, 20));

    for (const auto& patch : patches)
    {
        EXPECT_GE(patch.src.x, 0.0f);
        EXPECT_GE(patch.src.y, 0.0f);
        EXPECT_LE(patch.src.x + patch.src.width, 1.0001f);
        EXPECT_LE(patch.src.y + patch.src.height, 1.0001f);
    }
}

// -----------------------------------------------------------------------
// Invalid texture dimensions produce no patches rather than dividing by
// zero.
// -----------------------------------------------------------------------

TEST(NinePatchGeometry, ZeroSizeTextureProducesNoPatches)
{
    const auto patches = cw::computeNinePatchGeometry(
        0.0f, 0.0f,
        cw::Rect::fromLTWH(0, 0, 1, 1),
        cw::Rect::fromLTWH(0, 0, 10, 10));
    EXPECT_TRUE(patches.empty());
}
