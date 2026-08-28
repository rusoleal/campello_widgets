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

// -----------------------------------------------------------------------
// Paint::shader — general gradient fills for any drawXxx() call, wrapped in
// a beginShaderMask()/endShaderMask() scope internally.
// -----------------------------------------------------------------------

TEST(CanvasPaintShader, DrawRectWithShaderEmitsMaskBeginRectEndSequence)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p = cw::Paint::filled(cw::Color::black());
    p.shader = cw::Shader{cw::LinearGradient{
        .begin = {0.0f, 0.0f}, .end = {10.0f, 0.0f}, .colors = {cw::Color::red(), cw::Color::blue()}}};
    canvas.drawRect(cw::Rect::fromLTWH(5, 5, 10, 10), p);

    ASSERT_EQ(canvas.commands().size(), 3u);
    const auto& begin = std::get<cw::DrawShaderMaskBeginCmd>(canvas.commands()[0]);
    const auto& rect   = std::get<cw::DrawRectCmd>(canvas.commands()[1]);
    EXPECT_TRUE(std::holds_alternative<cw::DrawShaderMaskEndCmd>(canvas.commands()[2]));

    EXPECT_EQ(begin.blend_mode, cw::BlendMode::modulate);
    EXPECT_FLOAT_EQ(begin.bounds.x, 5.0f);
    EXPECT_FLOAT_EQ(begin.bounds.y, 5.0f);
    EXPECT_FLOAT_EQ(begin.bounds.width, 10.0f);
    EXPECT_FLOAT_EQ(begin.bounds.height, 10.0f);

    // The masked draw uses an opaque white fill (RGB replaced, original
    // alpha preserved) and no longer carries a shader (avoids re-wrapping).
    EXPECT_FLOAT_EQ(rect.paint.color.r, 1.0f);
    EXPECT_FLOAT_EQ(rect.paint.color.g, 1.0f);
    EXPECT_FLOAT_EQ(rect.paint.color.b, 1.0f);
    EXPECT_FLOAT_EQ(rect.paint.color.a, 1.0f);
    EXPECT_FALSE(rect.paint.shader.has_value());
}

TEST(CanvasPaintShader, FillBoundsAreNotInsetAndShaderOriginUnshifted)
{
    // A fill (PaintStyle::fill) has no stroke to clip -- the capture bounds
    // must exactly equal the shape's own bounds, and the shader's own
    // Offset coordinates must be passed through unchanged.
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p = cw::Paint::filled(cw::Color::black());
    p.shader = cw::Shader{cw::LinearGradient{
        .begin = {1.0f, 2.0f}, .end = {8.0f, 2.0f}, .colors = {cw::Color::red(), cw::Color::blue()}}};
    canvas.drawRect(cw::Rect::fromLTWH(20, 30, 10, 10), p);

    const auto& begin = std::get<cw::DrawShaderMaskBeginCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(begin.bounds.x, 20.0f);
    EXPECT_FLOAT_EQ(begin.bounds.y, 30.0f);
    EXPECT_FLOAT_EQ(begin.bounds.width, 10.0f);
    EXPECT_FLOAT_EQ(begin.bounds.height, 10.0f);

    const auto& grad = std::get<cw::LinearGradient>(begin.shader);
    EXPECT_FLOAT_EQ(grad.begin.x, 1.0f);
    EXPECT_FLOAT_EQ(grad.begin.y, 2.0f);
    EXPECT_FLOAT_EQ(grad.end.x, 8.0f);
    EXPECT_FLOAT_EQ(grad.end.y, 2.0f);
}

TEST(CanvasPaintShader, StrokeBoundsAreInsetByHalfStrokeWidthAndShaderShifted)
{
    // A PaintStyle::stroke draw's visible pixels extend stroke_width/2 past
    // the shape's own bounds -- the capture region must grow by that amount
    // (so the outer half of the stroke isn't clipped), and the shader's own
    // coordinates must shift by the same amount so they stay anchored to
    // the *unstroked* bounds, not the grown capture region.
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p = cw::Paint::stroked(cw::Color::black(), /*width=*/8.0f);
    p.shader = cw::Shader{cw::RadialGradient{
        .center = {5.0f, 5.0f}, .radius = 5.0f, .colors = {cw::Color::red(), cw::Color::blue()}}};
    canvas.drawRect(cw::Rect::fromLTWH(20, 30, 10, 10), p);

    const auto& begin = std::get<cw::DrawShaderMaskBeginCmd>(canvas.commands()[0]);
    // inset = stroke_width / 2 = 4.
    EXPECT_FLOAT_EQ(begin.bounds.x, 16.0f);
    EXPECT_FLOAT_EQ(begin.bounds.y, 26.0f);
    EXPECT_FLOAT_EQ(begin.bounds.width, 18.0f);
    EXPECT_FLOAT_EQ(begin.bounds.height, 18.0f);

    const auto& grad = std::get<cw::RadialGradient>(begin.shader);
    EXPECT_FLOAT_EQ(grad.center.x, 9.0f);  // 5 + inset(4)
    EXPECT_FLOAT_EQ(grad.center.y, 9.0f);
    EXPECT_FLOAT_EQ(grad.radius, 5.0f);    // radius itself is untouched

    const auto& rect_paint = std::get<cw::DrawRectCmd>(canvas.commands()[1]).paint;
    EXPECT_EQ(rect_paint.style, cw::PaintStyle::stroke);
    EXPECT_FLOAT_EQ(rect_paint.stroke_width, 8.0f);
}

TEST(CanvasPaintShader, PreservesColorAlphaAsOverallOpacityMultiplier)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p = cw::Paint::filled(cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.3f));
    p.shader = cw::Shader{cw::LinearGradient{
        .begin = {0, 0}, .end = {10, 0}, .colors = {cw::Color::red(), cw::Color::blue()}}};
    canvas.drawCircle({5, 5}, 5.0f, p);

    const auto& circle = std::get<cw::DrawCircleCmd>(canvas.commands()[1]);
    // RGB forced to white, alpha carried through from the original color.
    EXPECT_FLOAT_EQ(circle.paint.color.r, 1.0f);
    EXPECT_FLOAT_EQ(circle.paint.color.a, 0.3f);
}

TEST(CanvasPaintShader, DrawLineInsetsByHalfStrokeWidthRegardlessOfPaintStyle)
{
    // A line has no fill concept -- it must always be treated as
    // effectively stroked width-wise, even if the caller left
    // paint.style at its PaintStyle::fill default.
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color        = cw::Color::black();
    p.stroke_width = 6.0f; // style left at the default (fill)
    p.shader       = cw::Shader{cw::LinearGradient{
        .begin = {0, 0}, .end = {10, 0}, .colors = {cw::Color::red(), cw::Color::blue()}}};
    canvas.drawLine({10, 10}, {20, 10}, p);

    const auto& begin = std::get<cw::DrawShaderMaskBeginCmd>(canvas.commands()[0]);
    // inset = stroke_width / 2 = 3; line bbox is [10,20]x[10,10].
    EXPECT_FLOAT_EQ(begin.bounds.x, 7.0f);
    EXPECT_FLOAT_EQ(begin.bounds.y, 7.0f);
    EXPECT_FLOAT_EQ(begin.bounds.width, 16.0f);
    EXPECT_FLOAT_EQ(begin.bounds.height, 6.0f);
}

TEST(CanvasPaintShader, NoShaderTakesTheNormalPath)
{
    // Sanity check: unset shader means no extra ShaderMask commands at all.
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), cw::Paint::filled(cw::Color::black()));

    ASSERT_EQ(canvas.commands().size(), 1u);
    EXPECT_TRUE(std::holds_alternative<cw::DrawRectCmd>(canvas.commands()[0]));
}
