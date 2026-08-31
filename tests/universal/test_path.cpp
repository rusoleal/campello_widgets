#include <gtest/gtest.h>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <algorithm>
#include <cmath>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Splits a Path's flat command list back into per-contour point rings.
    // combine() only ever emits moveTo/lineTo/close, so this is a direct walk.
    std::vector<std::vector<cw::Offset>> extractContours(const cw::Path& path)
    {
        std::vector<std::vector<cw::Offset>> contours;
        for (const auto& cmd : path.commands())
        {
            switch (cmd.type)
            {
            case cw::Path::PathCommandType::moveTo:
                contours.emplace_back();
                contours.back().push_back(cmd.p1);
                break;
            case cw::Path::PathCommandType::lineTo:
                if (!contours.empty()) contours.back().push_back(cmd.p1);
                break;
            default:
                break;
            }
        }
        return contours;
    }

    // Shoelace formula. Positive/negative encodes winding direction.
    float signedArea(const std::vector<cw::Offset>& ring)
    {
        float area = 0.0f;
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const cw::Offset& p0 = ring[i];
            const cw::Offset& p1 = ring[(i + 1) % ring.size()];
            area += p0.x * p1.y - p1.x * p0.y;
        }
        return 0.5f * area;
    }

    float totalAbsArea(const cw::Path& path)
    {
        float total = 0.0f;
        for (const auto& ring : extractContours(path))
            total += std::abs(signedArea(ring));
        return total;
    }
}

// -----------------------------------------------------------------------
// unionOp
// -----------------------------------------------------------------------

TEST(PathCombine, UnionOfOverlappingRectsCoversBothBounds)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::unionOp, a, b);
    const cw::Rect bounds = result.getBounds();

    EXPECT_NEAR(bounds.left(),   0.0f, 1e-2f);
    EXPECT_NEAR(bounds.top(),    0.0f, 1e-2f);
    EXPECT_NEAR(bounds.right(),  15.0f, 1e-2f);
    EXPECT_NEAR(bounds.bottom(), 15.0f, 1e-2f);
    EXPECT_EQ(result.fillType(), cw::Path::FillType::winding);
}

TEST(PathCombine, UnionOfDisjointRectsProducesTwoContours)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 5.0f, 5.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(20.0f, 20.0f, 5.0f, 5.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::unionOp, a, b);
    EXPECT_EQ(extractContours(result).size(), 2u);
    EXPECT_NEAR(totalAbsArea(result), 50.0f, 1.0f);
}

TEST(PathCombine, UnionWithFullyContainedPathEqualsOuterAlone)
{
    cw::Path outer; outer.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 20.0f, 20.0f));
    cw::Path inner; inner.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 5.0f, 5.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::unionOp, outer, inner);
    EXPECT_EQ(extractContours(result).size(), 1u);
    EXPECT_NEAR(totalAbsArea(result), 400.0f, 1.0f);
}

TEST(PathCombine, UnionOfIdenticalRectsEqualsOneRect)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::unionOp, a, b);
    EXPECT_EQ(extractContours(result).size(), 1u);
    EXPECT_NEAR(totalAbsArea(result), 100.0f, 1.0f);
}

// -----------------------------------------------------------------------
// intersect
// -----------------------------------------------------------------------

TEST(PathCombine, IntersectOfOverlappingRectsIsOverlapRegion)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::intersect, a, b);
    const cw::Rect bounds = result.getBounds();

    EXPECT_NEAR(bounds.left(),   5.0f, 1e-2f);
    EXPECT_NEAR(bounds.top(),    5.0f, 1e-2f);
    EXPECT_NEAR(bounds.right(),  10.0f, 1e-2f);
    EXPECT_NEAR(bounds.bottom(), 10.0f, 1e-2f);
}

TEST(PathCombine, IntersectOfDisjointRectsIsEmpty)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 5.0f, 5.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(20.0f, 20.0f, 5.0f, 5.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::intersect, a, b);
    EXPECT_TRUE(result.isEmpty());
}

TEST(PathCombine, IntersectOfIdenticalRectsEqualsOneRect)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::intersect, a, b);
    EXPECT_EQ(extractContours(result).size(), 1u);
    EXPECT_NEAR(totalAbsArea(result), 100.0f, 1.0f);
}

// -----------------------------------------------------------------------
// difference / reverseDifference
// -----------------------------------------------------------------------

TEST(PathCombine, DifferenceProducesHoleWithTwoOppositelyWoundContours)
{
    cw::Path outer; outer.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 20.0f, 20.0f));
    cw::Path inner; inner.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::difference, outer, inner);
    const auto contours = extractContours(result);
    ASSERT_EQ(contours.size(), 2u);

    const float area0 = signedArea(contours[0]);
    const float area1 = signedArea(contours[1]);

    // A hole's ring winds opposite to its containing outer ring.
    EXPECT_TRUE((area0 > 0.0f) != (area1 > 0.0f));

    std::vector<float> magnitudes{std::abs(area0), std::abs(area1)};
    std::sort(magnitudes.begin(), magnitudes.end());
    EXPECT_NEAR(magnitudes[0], 100.0f, 1.0f);  // the hole (10x10)
    EXPECT_NEAR(magnitudes[1], 400.0f, 1.0f);  // the outer boundary (20x20)
}

TEST(PathCombine, DifferenceOfIdenticalRectsIsEmpty)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::difference, a, b);
    EXPECT_TRUE(result.isEmpty());
}

TEST(PathCombine, ReverseDifferenceMirrorsSwappedDifference)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));

    cw::Path reverse_diff = cw::Path::combine(cw::Path::PathOperation::reverseDifference, a, b);
    cw::Path swapped_diff = cw::Path::combine(cw::Path::PathOperation::difference, b, a);

    const cw::Rect rb = reverse_diff.getBounds();
    const cw::Rect sb = swapped_diff.getBounds();
    EXPECT_NEAR(rb.left(),   sb.left(),   1e-2f);
    EXPECT_NEAR(rb.top(),    sb.top(),    1e-2f);
    EXPECT_NEAR(rb.right(),  sb.right(),  1e-2f);
    EXPECT_NEAR(rb.bottom(), sb.bottom(), 1e-2f);
    EXPECT_NEAR(totalAbsArea(reverse_diff), totalAbsArea(swapped_diff), 1.0f);
}

// -----------------------------------------------------------------------
// xorOp
// -----------------------------------------------------------------------

TEST(PathCombine, XorIsSymmetricDifference)
{
    // Two 10x10 rects overlapping in a 5x5 region: xor area =
    // area(a) + area(b) - 2*area(intersection) = 100 + 100 - 2*25 = 150.
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::xorOp, a, b);
    EXPECT_NEAR(totalAbsArea(result), 150.0f, 1.0f);
}

TEST(PathCombine, XorOfIdenticalRectsIsEmpty)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    cw::Path b; b.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::xorOp, a, b);
    EXPECT_TRUE(result.isEmpty());
}

// -----------------------------------------------------------------------
// fillType handling
// -----------------------------------------------------------------------

TEST(PathCombine, MixedFillTypeInputsStillProduceWindingResult)
{
    cw::Path a; a.addRect(cw::Rect::fromLTWH(0.0f, 0.0f, 10.0f, 10.0f));
    a.setFillType(cw::Path::FillType::evenOdd);
    cw::Path b; b.addRect(cw::Rect::fromLTWH(5.0f, 5.0f, 10.0f, 10.0f));
    ASSERT_EQ(b.fillType(), cw::Path::FillType::winding);

    cw::Path result = cw::Path::combine(cw::Path::PathOperation::unionOp, a, b);
    EXPECT_EQ(result.fillType(), cw::Path::FillType::winding);

    const cw::Rect bounds = result.getBounds();
    EXPECT_NEAR(bounds.right(),  15.0f, 1e-2f);
    EXPECT_NEAR(bounds.bottom(), 15.0f, 1e-2f);
}
