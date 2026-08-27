#include <gtest/gtest.h>
#include <campello_widgets/ui/box_gradient.hpp>
#include <campello_widgets/ui/box_border.hpp>
#include <campello_widgets/ui/box_decoration.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// resolveBoxGradient — LinearBoxGradient
// -----------------------------------------------------------------------

TEST(ResolveBoxGradient, LinearTopLeftToBottomRight)
{
    cw::LinearBoxGradient g{
        .begin  = cw::Alignment::topLeft(),
        .end    = cw::Alignment::bottomRight(),
        .colors = {cw::Color::black(), cw::Color::white()},
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{200.0f, 100.0f});
    ASSERT_TRUE(std::holds_alternative<cw::LinearGradient>(shader));

    const auto& linear = std::get<cw::LinearGradient>(shader);
    EXPECT_FLOAT_EQ(linear.begin.x, 0.0f);
    EXPECT_FLOAT_EQ(linear.begin.y, 0.0f);
    EXPECT_FLOAT_EQ(linear.end.x, 200.0f);
    EXPECT_FLOAT_EQ(linear.end.y, 100.0f);
    EXPECT_EQ(linear.colors.size(), 2u);
    EXPECT_EQ(linear.colors[0], cw::Color::black());
    EXPECT_EQ(linear.colors[1], cw::Color::white());
}

TEST(ResolveBoxGradient, LinearCenterAlignmentIsBoxMidpoint)
{
    cw::LinearBoxGradient g{
        .begin  = cw::Alignment::centerLeft(),
        .end    = cw::Alignment::center(),
        .colors = {cw::Color::black(), cw::Color::white()},
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{200.0f, 100.0f});
    const auto&       linear = std::get<cw::LinearGradient>(shader);

    EXPECT_FLOAT_EQ(linear.begin.x, 0.0f);
    EXPECT_FLOAT_EQ(linear.begin.y, 50.0f);
    EXPECT_FLOAT_EQ(linear.end.x, 100.0f);
    EXPECT_FLOAT_EQ(linear.end.y, 50.0f);
}

TEST(ResolveBoxGradient, LinearPassesThroughStopsAndTileMode)
{
    cw::LinearBoxGradient g{
        .colors    = {cw::Color::black(), cw::Color::white(), cw::Color::black()},
        .stops     = {0.0f, 0.3f, 1.0f},
        .tile_mode = cw::TileMode::repeated,
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{50.0f, 50.0f});
    const auto& linear = std::get<cw::LinearGradient>(shader);
    ASSERT_EQ(linear.stops.size(), 3u);
    EXPECT_FLOAT_EQ(linear.stops[1], 0.3f);
    EXPECT_EQ(linear.tile_mode, cw::TileMode::repeated);
}

// -----------------------------------------------------------------------
// resolveBoxGradient — RadialBoxGradient
// -----------------------------------------------------------------------

TEST(ResolveBoxGradient, RadialCenterDefaultsToBoxCenter)
{
    cw::RadialBoxGradient g{
        .colors = {cw::Color::white(), cw::Color::black()},
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{200.0f, 100.0f});
    const auto& radial = std::get<cw::RadialGradient>(shader);
    EXPECT_FLOAT_EQ(radial.center.x, 100.0f);
    EXPECT_FLOAT_EQ(radial.center.y, 50.0f);
}

TEST(ResolveBoxGradient, RadialRadiusIsFractionOfShortestSide)
{
    cw::RadialBoxGradient g{
        .radius = 0.5f,
        .colors = {cw::Color::white(), cw::Color::black()},
    };

    // Wide box: shortest side is height (100), so radius = 0.5 * 100 = 50.
    const cw::Shader wide_shader = cw::resolveBoxGradient(g, cw::Size{200.0f, 100.0f});
    EXPECT_FLOAT_EQ(std::get<cw::RadialGradient>(wide_shader).radius, 50.0f);

    // Tall box: shortest side is width (80), so radius = 0.5 * 80 = 40.
    const cw::Shader tall_shader = cw::resolveBoxGradient(g, cw::Size{80.0f, 300.0f});
    EXPECT_FLOAT_EQ(std::get<cw::RadialGradient>(tall_shader).radius, 40.0f);
}

TEST(ResolveBoxGradient, RadialCustomCenterAlignment)
{
    cw::RadialBoxGradient g{
        .center = cw::Alignment::topLeft(),
        .radius = 1.0f,
        .colors = {cw::Color::white(), cw::Color::black()},
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{100.0f, 60.0f});
    const auto& radial = std::get<cw::RadialGradient>(shader);
    EXPECT_FLOAT_EQ(radial.center.x, 0.0f);
    EXPECT_FLOAT_EQ(radial.center.y, 0.0f);
    EXPECT_FLOAT_EQ(radial.radius, 60.0f); // shortest side (height) * 1.0
}

// -----------------------------------------------------------------------
// resolveBoxGradient — SweepBoxGradient
// -----------------------------------------------------------------------

TEST(ResolveBoxGradient, SweepDefaultsToFullCircleAtBoxCenter)
{
    cw::SweepBoxGradient g{
        .colors = {cw::Color::black(), cw::Color::white(), cw::Color::black()},
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{120.0f, 80.0f});
    const auto& sweep = std::get<cw::SweepGradient>(shader);
    EXPECT_FLOAT_EQ(sweep.center.x, 60.0f);
    EXPECT_FLOAT_EQ(sweep.center.y, 40.0f);
    EXPECT_FLOAT_EQ(sweep.start_angle, 0.0f);
    EXPECT_NEAR(sweep.end_angle, 2.0f * 3.14159265f, 1e-4f);
    EXPECT_EQ(sweep.colors.size(), 3u);
}

TEST(ResolveBoxGradient, SweepCustomAngleRangePassesThrough)
{
    cw::SweepBoxGradient g{
        .start_angle = 0.5f,
        .end_angle   = 3.0f,
        .colors      = {cw::Color::black(), cw::Color::white()},
        .tile_mode   = cw::TileMode::mirror,
    };

    const cw::Shader shader = cw::resolveBoxGradient(g, cw::Size{40.0f, 40.0f});
    const auto& sweep = std::get<cw::SweepGradient>(shader);
    EXPECT_FLOAT_EQ(sweep.start_angle, 0.5f);
    EXPECT_FLOAT_EQ(sweep.end_angle, 3.0f);
    EXPECT_EQ(sweep.tile_mode, cw::TileMode::mirror);
}

// -----------------------------------------------------------------------
// BoxGradient variant selection — resolveBoxGradient dispatches to the
// alternative actually held by the BoxGradient variant, not always Linear.
// -----------------------------------------------------------------------

TEST(ResolveBoxGradient, VariantDispatchSelectsCorrectShaderAlternative)
{
    const cw::BoxGradient linear_g = cw::LinearBoxGradient{.colors = {cw::Color::black(), cw::Color::white()}};
    const cw::BoxGradient radial_g = cw::RadialBoxGradient{.colors = {cw::Color::black(), cw::Color::white()}};
    const cw::BoxGradient sweep_g  = cw::SweepBoxGradient{.colors = {cw::Color::black(), cw::Color::white()}};

    EXPECT_TRUE(std::holds_alternative<cw::LinearGradient>(cw::resolveBoxGradient(linear_g, cw::Size{10, 10})));
    EXPECT_TRUE(std::holds_alternative<cw::RadialGradient>(cw::resolveBoxGradient(radial_g, cw::Size{10, 10})));
    EXPECT_TRUE(std::holds_alternative<cw::SweepGradient>(cw::resolveBoxGradient(sweep_g, cw::Size{10, 10})));
}

// -----------------------------------------------------------------------
// BoxBorder::gradientBorder() / BoxDecoration.gradient
// -----------------------------------------------------------------------

TEST(BoxBorder, GradientBorderFactorySetsGradientAndWidth)
{
    const cw::LinearBoxGradient grad{.colors = {cw::Color::black(), cw::Color::white()}};
    const cw::BoxBorder border = cw::BoxBorder::gradientBorder(grad, 3.0f);

    ASSERT_TRUE(border.gradient.has_value());
    EXPECT_FLOAT_EQ(border.width, 3.0f);
}

TEST(BoxBorder, SolidBorderHasNoGradient)
{
    const cw::BoxBorder border = cw::BoxBorder::all(cw::Color::black(), 2.0f);
    EXPECT_FALSE(border.gradient.has_value());
}

TEST(BoxDecoration, GradientAndSolidColorAreIndependentFields)
{
    cw::BoxDecoration deco;
    deco.color    = cw::Color::black();
    deco.gradient = cw::LinearBoxGradient{.colors = {cw::Color::black(), cw::Color::white()}};

    // Both fields can be set independently on the struct itself -- the
    // "gradient wins" precedence is a paint-time decision (RenderDecoratedBox),
    // not something BoxDecoration enforces structurally.
    EXPECT_TRUE(deco.color.has_value());
    EXPECT_TRUE(deco.gradient.has_value());
}
