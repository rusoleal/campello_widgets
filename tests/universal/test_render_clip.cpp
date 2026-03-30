#include <gtest/gtest.h>
#include <campello_widgets/ui/render_clip_rect.hpp>
#include <campello_widgets/ui/render_clip_rrect.hpp>
#include <campello_widgets/ui/render_clip_oval.hpp>
#include <campello_widgets/ui/render_clip_path.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// =========================================================================
// RenderClipRect
// =========================================================================

TEST(RenderClipRect, NoChildIsZeroSize)
{
    // No child → constrain(zero) under loose constraints gives 0×0.
    cw::RenderClipRect clip;
    clip.layout(cw::BoxConstraints::loose(300, 200));
    EXPECT_FLOAT_EQ(clip.size().width,  0.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 0.0f);
}

TEST(RenderClipRect, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 120.0f;
    child->height = 80.0f;

    cw::RenderClipRect clip;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(300, 200));

    EXPECT_FLOAT_EQ(clip.size().width,  120.0f);
    EXPECT_FLOAT_EQ(clip.size().height,  80.0f);
}

TEST(RenderClipRect, ChildClampedToParentMax)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderClipRect clip;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(100, 75));

    EXPECT_FLOAT_EQ(clip.size().width,  100.0f);
    EXPECT_FLOAT_EQ(clip.size().height,  75.0f);
}

// =========================================================================
// RenderClipRRect
// =========================================================================

TEST(RenderClipRRect, NoChildIsZeroSize)
{
    cw::RenderClipRRect clip;
    clip.border_radius = 8.0f;
    clip.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(clip.size().width,  0.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 0.0f);
}

TEST(RenderClipRRect, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 150.0f;
    child->height = 100.0f;

    cw::RenderClipRRect clip;
    clip.border_radius = 12.0f;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(clip.size().width,  150.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 100.0f);
}

TEST(RenderClipRRect, ZeroRadiusBehavesLikeClipRect)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 80.0f;
    child->height = 60.0f;

    cw::RenderClipRRect clip;
    clip.border_radius = 0.0f;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(200, 200));

    EXPECT_FLOAT_EQ(clip.size().width,  80.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 60.0f);
}

// =========================================================================
// RenderClipOval
// =========================================================================

TEST(RenderClipOval, NoChildIsZeroSize)
{
    cw::RenderClipOval clip;
    clip.layout(cw::BoxConstraints::loose(200, 200));
    EXPECT_FLOAT_EQ(clip.size().width,  0.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 0.0f);
}

TEST(RenderClipOval, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 100.0f;

    cw::RenderClipOval clip;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(300, 300));

    EXPECT_FLOAT_EQ(clip.size().width,  100.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 100.0f);
}

TEST(RenderClipOval, RectangularChildProducesRectangularSize)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 200.0f;
    child->height = 100.0f;

    cw::RenderClipOval clip;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(clip.size().width,  200.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 100.0f);
}

// =========================================================================
// RenderClipPath
// =========================================================================

TEST(RenderClipPath, NoChildIsZeroSize)
{
    // No child → constrain(zero) under loose constraints gives 0×0.
    cw::RenderClipPath clip;
    clip.clip_path_builder = [](cw::Size sz) {
        cw::Path p;
        p.moveTo(0, 0);
        p.lineTo(sz.width, 0);
        p.lineTo(sz.width, sz.height);
        p.close();
        return p;
    };
    clip.layout(cw::BoxConstraints::loose(250, 180));
    EXPECT_FLOAT_EQ(clip.size().width,  0.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 0.0f);
}

TEST(RenderClipPath, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 90.0f;
    child->height = 70.0f;

    cw::RenderClipPath clip;
    clip.clip_path_builder = [](cw::Size sz) {
        cw::Path p;
        p.moveTo(sz.width * 0.5f, 0);
        p.lineTo(sz.width, sz.height);
        p.lineTo(0, sz.height);
        p.close();
        return p;
    };
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(300, 300));

    EXPECT_FLOAT_EQ(clip.size().width,  90.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 70.0f);
}

TEST(RenderClipPath, NullBuilderStillLaysOutCorrectly)
{
    // No builder set — layout should still work (paint would be a no-op).
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 50.0f;
    child->height = 50.0f;

    cw::RenderClipPath clip;
    clip.setChild(child);
    clip.layout(cw::BoxConstraints::loose(200, 200));

    EXPECT_FLOAT_EQ(clip.size().width,  50.0f);
    EXPECT_FLOAT_EQ(clip.size().height, 50.0f);
}
