#include <gtest/gtest.h>
#include <campello_widgets/ui/dirty_region.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(DirtyRegion, NoBackdropRegionsIsNeverDirty)
{
    // No BackdropFilter painted this frame — nothing to gate.
    EXPECT_FALSE(cw::anyRegionDirty({}, 5.0f, 5.0f, {cw::Rect::fromLTWH(0, 0, 100, 100)}, false));
}

TEST(DirtyRegion, NoDirtyRectsIsNotDirty)
{
    // A BackdropFilter exists, but nothing painted anywhere this frame.
    const std::vector<cw::Rect> regions{cw::Rect::fromLTWH(50, 50, 100, 100)};
    EXPECT_FALSE(cw::anyRegionDirty(regions, 5.0f, 5.0f, {}, false));
}

TEST(DirtyRegion, DistantDirtyRectDoesNotTrigger)
{
    const std::vector<cw::Rect> regions{cw::Rect::fromLTWH(0, 0, 50, 50)};
    const std::vector<cw::Rect> dirty{cw::Rect::fromLTWH(500, 500, 20, 20)};
    EXPECT_FALSE(cw::anyRegionDirty(regions, 5.0f, 5.0f, dirty, false));
}

TEST(DirtyRegion, OverlappingDirtyRectTriggers)
{
    const std::vector<cw::Rect> regions{cw::Rect::fromLTWH(0, 0, 50, 50)};
    const std::vector<cw::Rect> dirty{cw::Rect::fromLTWH(40, 40, 20, 20)};
    EXPECT_TRUE(cw::anyRegionDirty(regions, 5.0f, 5.0f, dirty, false));
}

TEST(DirtyRegion, DirtyRectOnlyWithinMarginTriggers)
{
    // Dirty rect sits just outside the exact backdrop bounds, but within
    // the blur margin — must still trigger, since blur samples that far.
    const std::vector<cw::Rect> regions{cw::Rect::fromLTWH(0, 0, 50, 50)};
    const std::vector<cw::Rect> dirty{cw::Rect::fromLTWH(53, 10, 5, 5)}; // 3px past the edge
    EXPECT_FALSE(cw::anyRegionDirty(regions, 2.0f, 2.0f, dirty, false))
        << "margin (2px) is smaller than the gap (3px) — must not trigger";
    EXPECT_TRUE(cw::anyRegionDirty(regions, 5.0f, 5.0f, dirty, false))
        << "margin (5px) covers the gap (3px) — must trigger";
}

TEST(DirtyRegion, OverflowedAlwaysTriggersWhenRegionsExist)
{
    const std::vector<cw::Rect> regions{cw::Rect::fromLTWH(0, 0, 50, 50)};
    EXPECT_TRUE(cw::anyRegionDirty(regions, 5.0f, 5.0f, {}, /*dirty_overflowed=*/true));
}

TEST(DirtyRegion, OverflowedWithNoRegionsIsStillNotDirty)
{
    EXPECT_FALSE(cw::anyRegionDirty({}, 5.0f, 5.0f, {}, /*dirty_overflowed=*/true));
}

// ---------------------------------------------------------------------------
// projectedBounds() — regression coverage for "BackdropFilter inside a
// scroll shows mismatched/stale content": offset-based positioning and an
// ambient canvas transform (a scroll's canvas.translate(), in particular)
// are independent and additive, so a rect built from offset+size alone is
// only the *logical* position, not the true on-screen position. Every
// dirty-region reporter needs to project through the current transform so
// all reported bounds are comparable in one consistent coordinate space —
// see the function's own doc comment for the full explanation.
// ---------------------------------------------------------------------------

TEST(ProjectedBounds, IdentityTransformIsNoOp)
{
    const cw::Rect local = cw::Rect::fromLTWH(10.0f, 20.0f, 30.0f, 40.0f);
    const cw::Rect result = cw::projectedBounds(cw::Matrix4::identity(), local);

    EXPECT_FLOAT_EQ(result.x, local.x);
    EXPECT_FLOAT_EQ(result.y, local.y);
    EXPECT_FLOAT_EQ(result.width, local.width);
    EXPECT_FLOAT_EQ(result.height, local.height);
}

TEST(ProjectedBounds, TranslationShiftsBounds)
{
    // Mirrors RenderSingleChildScrollView::performPaint()'s
    // canvas.translate(0, -scroll) — the transform a scrolled
    // BackdropFilter's ambient canvas carries at paint time.
    const cw::Rect   local     = cw::Rect::fromLTWH(0.0f, 1200.0f, 300.0f, 200.0f);
    const cw::Matrix4 scrolled = cw::Matrix4::translate({0.0f, -700.0f, 0.0f});

    const cw::Rect result = cw::projectedBounds(scrolled, local);

    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 500.0f)
        << "must reflect the true on-screen position after the scroll "
           "translate, not the logical/unscrolled offset";
    EXPECT_FLOAT_EQ(result.width, 300.0f);
    EXPECT_FLOAT_EQ(result.height, 200.0f);
}

TEST(ProjectedBounds, WithoutProjectionScrolledBackdropFilterWouldMissTheScrollViewsOwnDirtyReport)
{
    // The actual bug this fixes: a BackdropFilter's logical bounds never
    // change while scrolling (RenderSingleChildScrollView never adjusts
    // `offset`, only its canvas transform), while the scroll view's own
    // dirty report — see OffsetLayer::maybeReplay()'s `dirty` branch —
    // correctly uses its own true, unscrolled screen position (the
    // scrollable's own on-screen position never changes; only its
    // *content* does). Without projecting the filter's bounds through the
    // ambient transform, these two reports live in different coordinate
    // spaces and can fail to intersect even though the filter is plainly
    // inside the scroll view's viewport.
    const cw::Rect filter_local_bounds = cw::Rect::fromLTWH(0.0f, 1200.0f, 300.0f, 200.0f);
    const cw::Rect scroll_view_viewport = cw::Rect::fromLTWH(0.0f, 100.0f, 300.0f, 600.0f);

    ASSERT_FALSE(filter_local_bounds.intersects(scroll_view_viewport))
        << "sanity check: the logical/unprojected bounds land far outside "
           "the viewport, exactly the coordinate-space mismatch";

    const cw::Matrix4 scrolled = cw::Matrix4::translate({0.0f, -700.0f, 0.0f});
    const cw::Rect projected = cw::projectedBounds(scrolled, filter_local_bounds);

    EXPECT_TRUE(projected.intersects(scroll_view_viewport))
        << "once projected through the same ambient transform, the "
           "filter's true on-screen bounds correctly fall inside the "
           "scroll view's own reported dirty viewport";
}
