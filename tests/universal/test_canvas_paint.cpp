#include <gtest/gtest.h>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Paint::invert_colors — resolved on the CPU when the command is recorded
// -----------------------------------------------------------------------

TEST(CanvasPaint, InvertColorsFlipsColorAndClearsFlagOnDrawRect)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color         = cw::Color::fromRGBA(1.0f, 0.0f, 0.25f, 0.5f);
    p.invert_colors = true;
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(cmd.paint.color.r, 0.0f);
    EXPECT_FLOAT_EQ(cmd.paint.color.g, 1.0f);
    EXPECT_FLOAT_EQ(cmd.paint.color.b, 0.75f);
    EXPECT_FLOAT_EQ(cmd.paint.color.a, 0.5f); // alpha untouched by inversion
    EXPECT_FALSE(cmd.paint.invert_colors);    // already applied -- consumed
}

TEST(CanvasPaint, NoInversionWhenFlagUnset)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGB(0.2f, 0.4f, 0.6f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(cmd.paint.color.r, 0.2f);
    EXPECT_FLOAT_EQ(cmd.paint.color.g, 0.4f);
    EXPECT_FLOAT_EQ(cmd.paint.color.b, 0.6f);
}

TEST(CanvasPaint, InvertColorsAppliesAcrossShapeDrawCalls)
{
    // Spot-check a couple of the other Paint-taking draw methods to confirm
    // the resolution isn't drawRect()-specific.
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color         = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
    p.invert_colors = true;
    canvas.drawCircle({5, 5}, 5.0f, p);
    canvas.drawRRect(cw::RRect{cw::Rect::fromLTWH(0, 0, 10, 10), 2.0f}, p);

    const auto& circle_cmd = std::get<cw::DrawCircleCmd>(canvas.commands()[0]);
    const auto& rrect_cmd  = std::get<cw::DrawRRectCmd>(canvas.commands()[1]);
    EXPECT_FLOAT_EQ(circle_cmd.paint.color.r, 0.0f);
    EXPECT_FLOAT_EQ(rrect_cmd.paint.color.r, 0.0f);
}

// -----------------------------------------------------------------------
// Regression: drawPaint()/drawColor() used to double-apply opacity (once
// in their own body, once again inside the drawRect() they delegate to).
// -----------------------------------------------------------------------

TEST(CanvasPaint, DrawColorAppliesOpacityExactlyOnce)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.setOpacity(0.5f);
    canvas.drawColor(cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f), cw::BlendMode::srcOver);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(cmd.paint.color.a, 0.5f); // not 0.25f (0.5 * 0.5)
}

TEST(CanvasPaint, DrawPaintAppliesOpacityExactlyOnce)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.setOpacity(0.5f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    canvas.drawPaint(p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(cmd.paint.color.a, 0.5f); // not 0.25f
}

// -----------------------------------------------------------------------
// FilterQuality — threaded through drawImage()/drawTintedImage()
// -----------------------------------------------------------------------

TEST(CanvasPaint, DrawImageDefaultsToHighFilterQuality)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.drawImage(nullptr, cw::Rect::fromLTWH(0, 0, 1, 1), cw::Rect::fromLTWH(0, 0, 10, 10));

    const auto& cmd = std::get<cw::DrawImageCmd>(canvas.commands()[0]);
    EXPECT_EQ(cmd.filter_quality, cw::FilterQuality::high);
}

TEST(CanvasPaint, DrawImagePassesThroughRequestedFilterQuality)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.drawImage(nullptr, cw::Rect::fromLTWH(0, 0, 1, 1), cw::Rect::fromLTWH(0, 0, 10, 10),
                      cw::FilterQuality::none);

    const auto& cmd = std::get<cw::DrawImageCmd>(canvas.commands()[0]);
    EXPECT_EQ(cmd.filter_quality, cw::FilterQuality::none);
}

TEST(CanvasPaint, DrawTintedImagePassesThroughRequestedFilterQuality)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.drawTintedImage(nullptr, cw::Rect::fromLTWH(0, 0, 1, 1), cw::Rect::fromLTWH(0, 0, 10, 10),
                            cw::Color::black(), cw::FilterQuality::none);

    const auto& cmd = std::get<cw::DrawTintedImageCmd>(canvas.commands()[0]);
    EXPECT_EQ(cmd.filter_quality, cw::FilterQuality::none);
}
