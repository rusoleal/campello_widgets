#include <gtest/gtest.h>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Paint::color_filter -- resolved on the CPU when the command is recorded,
// same shape as invert_colors's existing tests (test_canvas_paint.cpp).
// -----------------------------------------------------------------------

TEST(ColorFilter, SrcInReplacesRgbAndScalesAlphaByCoverage)
{
    // srcIn: the dominant real-world use (icon-style tinting) -- the
    // filtered result's RGB is exactly the filter color, independent of
    // the original paint color, with alpha = filter.a * original coverage.
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.5f); // arbitrary original
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(1.0f, 0.0f, 0.0f, 1.0f), cw::BlendMode::srcIn};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_FLOAT_EQ(cmd.paint.color.r, 1.0f);
    EXPECT_FLOAT_EQ(cmd.paint.color.g, 0.0f);
    EXPECT_FLOAT_EQ(cmd.paint.color.b, 0.0f);
    EXPECT_NEAR(cmd.paint.color.a, 0.5f, 1e-5f); // filter.a(1.0) * original coverage(0.5)
    EXPECT_FALSE(cmd.paint.color_filter.has_value()); // consumed
}

TEST(ColorFilter, SrcReplacesEverythingWithTheFilterColor)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.9f);
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(0.0f, 1.0f, 0.0f, 0.7f), cw::BlendMode::src};
    canvas.drawCircle({5, 5}, 5.0f, p);

    const auto& cmd = std::get<cw::DrawCircleCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.r, 0.0f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.g, 1.0f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.b, 0.0f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.a, 0.7f, 1e-5f);
}

TEST(ColorFilter, DstIgnoresTheFilterAndKeepsTheOriginal)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.9f);
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f), cw::BlendMode::dst};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.r, 0.2f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.g, 0.4f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.b, 0.6f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.a, 0.9f, 1e-5f);
}

TEST(ColorFilter, ClearProducesFullyTransparentBlack)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 0.9f);
    p.color_filter = cw::ColorFilterMode{cw::Color::white(), cw::BlendMode::clear};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.a, 0.0f, 1e-5f);
}

TEST(ColorFilter, ModulateMultipliesComponentwise)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.5f, 1.0f, 0.2f, 1.0f);
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(1.0f, 0.5f, 0.5f, 1.0f), cw::BlendMode::modulate};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.r, 0.5f, 1e-5f);  // 1.0 * 0.5
    EXPECT_NEAR(cmd.paint.color.g, 0.5f, 1e-5f);  // 0.5 * 1.0
    EXPECT_NEAR(cmd.paint.color.b, 0.1f, 1e-5f);  // 0.5 * 0.2
    EXPECT_NEAR(cmd.paint.color.a, 1.0f, 1e-5f);
}

TEST(ColorFilter, PlusAddsAndClamps)
{
    cw::Canvas canvas(400.0f, 300.0f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.8f, 0.1f, 0.0f, 1.0f);
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(0.5f, 0.1f, 0.0f, 1.0f), cw::BlendMode::plus};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.r, 1.0f, 1e-5f); // 0.8+0.5 clamped to 1
    EXPECT_NEAR(cmd.paint.color.g, 0.2f, 1e-5f); // 0.1+0.1
}

TEST(ColorFilter, NoFilterTakesTheNormalPath)
{
    cw::Canvas canvas(400.0f, 300.0f);
    cw::Paint p = cw::Paint::filled(cw::Color::fromRGB(0.1f, 0.2f, 0.3f));
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    EXPECT_NEAR(cmd.paint.color.r, 0.1f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.g, 0.2f, 1e-5f);
    EXPECT_NEAR(cmd.paint.color.b, 0.3f, 1e-5f);
    EXPECT_FALSE(cmd.paint.color_filter.has_value());
}

TEST(ColorFilter, ComposesWithCanvasOpacityAfterResolution)
{
    cw::Canvas canvas(400.0f, 300.0f);
    canvas.setOpacity(0.5f);

    cw::Paint p;
    p.color = cw::Color::fromRGBA(0.2f, 0.4f, 0.6f, 1.0f);
    p.color_filter = cw::ColorFilterMode{cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 1.0f), cw::BlendMode::srcIn};
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 10, 10), p);

    const auto& cmd = std::get<cw::DrawRectCmd>(canvas.commands()[0]);
    // srcIn -> alpha = 1.0(filter.a) * 1.0(original coverage) = 1.0, then
    // canvas opacity(0.5) applies on top.
    EXPECT_NEAR(cmd.paint.color.a, 0.5f, 1e-5f);
}
