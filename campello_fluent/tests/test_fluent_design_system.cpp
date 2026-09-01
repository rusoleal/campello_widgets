#include <gtest/gtest.h>

#include <campello_fluent/fluent_design_system.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/text.hpp>

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Token factories
// ---------------------------------------------------------------------------

TEST(FluentDesignSystem, LightTokensAreValid)
{
    auto ds = FluentDesignSystem::light();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::light);
    // Default Fluent accent is Windows blue (0xFF0078D4) -- no red channel,
    // so assert on blue/alpha rather than assuming a warm palette like
    // CampelloDesignSystem's.
    EXPECT_GT(t.colors.primary.b, 0.0f);
    EXPECT_GT(t.colors.primary.a, 0.0f);
    EXPECT_EQ(t.colors.on_primary, Color::white());
    EXPECT_GT(t.elevation.level1, 0.0f);
    EXPECT_GT(t.shape.radius_md, 0.0f);
    EXPECT_GT(t.spacing.md, 0.0f);
    EXPECT_GT(t.motion.duration_normal, 0.0);
}

TEST(FluentDesignSystem, DefaultConstructorIsLight)
{
    FluentDesignSystem ds;
    EXPECT_EQ(ds.tokens().brightness, Brightness::light);
}

// ---------------------------------------------------------------------------
// Component builders — smoke tests
// ---------------------------------------------------------------------------

TEST(FluentDesignSystem, BuildButtonReturnsWidget)
{
    auto ds = FluentDesignSystem::light();

    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("OK");
    cfg.on_pressed = [] {};

    auto w = ds.buildButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(FluentDesignSystem, BuildProgressIndicatorCircularReturnsWidget)
{
    auto ds = FluentDesignSystem::light();

    ProgressConfig cfg;
    cfg.type = ProgressType::circular;

    auto w = ds.buildProgressIndicator(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(FluentDesignSystem, BuildRefreshIndicatorReturnsWidget)
{
    auto ds = FluentDesignSystem::light();

    RefreshIndicatorConfig cfg;
    cfg.pull_progress = 0.5f;

    auto w = ds.buildRefreshIndicator(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(FluentDesignSystem, BuildRefreshIndicatorRefreshingReturnsWidget)
{
    auto ds = FluentDesignSystem::light();

    RefreshIndicatorConfig cfg;
    cfg.refreshing = true;

    auto w = ds.buildRefreshIndicator(cfg);
    EXPECT_NE(w, nullptr);
}
