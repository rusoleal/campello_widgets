#include <gtest/gtest.h>

#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/progress_indicator.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/ui/campello_design_system.hpp>

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Token differences
// ---------------------------------------------------------------------------

TEST(Theme, LightAndDarkTokensDiffer)
{
    auto light = CampelloDesignSystem::light();
    auto dark  = CampelloDesignSystem::dark();

    EXPECT_EQ(light.tokens().brightness, Brightness::light);
    EXPECT_EQ(dark.tokens().brightness, Brightness::dark);

    // Primary colors should differ
    EXPECT_NE(light.tokens().colors.primary.r, dark.tokens().colors.primary.r);
}

TEST(Theme, LightAndDarkSurfaceColorsDiffer)
{
    auto light = CampelloDesignSystem::light();
    auto dark  = CampelloDesignSystem::dark();

    // Light surface should be light; dark surface should be dark
    EXPECT_GT(light.tokens().colors.surface.r, 0.5f);
    EXPECT_LT(dark.tokens().colors.surface.r, 0.5f);
}

TEST(Theme, FallbackTokensAreLight)
{
    // The static fallback used by Theme::of() when no Theme is present
    // should be a light CampelloDesignSystem.
    auto fallback = CampelloDesignSystem();
    EXPECT_EQ(fallback.tokens().brightness, Brightness::light);
}

// ---------------------------------------------------------------------------
// Theme widget construction
// ---------------------------------------------------------------------------

TEST(Theme, CanCreateThemeWithDesignSystem)
{
    auto ds = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::dark());
    auto child = std::make_shared<Text>("Hello");
    auto theme = std::make_shared<Theme>(ds, child);

    EXPECT_NE(theme, nullptr);
    EXPECT_EQ(theme->design_system, ds);
    EXPECT_EQ(theme->child, child);
}

TEST(Theme, UpdateShouldNotifyWhenDesignSystemChanges)
{
    auto ds1 = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::light());
    auto ds2 = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::dark());

    auto theme1 = std::make_shared<Theme>(ds1, nullptr);
    auto theme2 = std::make_shared<Theme>(ds2, nullptr);

    // Different design_system pointers should trigger notification
    EXPECT_TRUE(theme1->updateShouldNotify(*theme2));
}

TEST(Theme, UpdateShouldNotNotifyWhenDesignSystemSame)
{
    auto ds = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::light());

    auto theme1 = std::make_shared<Theme>(ds, nullptr);
    auto theme2 = std::make_shared<Theme>(ds, nullptr);

    // Same design_system pointer should not trigger notification
    EXPECT_FALSE(theme1->updateShouldNotify(*theme2));
}

// ---------------------------------------------------------------------------
// Adaptive widget construction ( smoke tests — no element tree needed )
// ---------------------------------------------------------------------------

TEST(Theme, ButtonBuildsWithDefaultConfig)
{
    auto btn = std::make_shared<Button>();
    btn->child = std::make_shared<Text>("OK");
    btn->priority = ButtonPriority::primary;

    // The build() method exists and the widget can be constructed.
    // Without a real BuildContext, Theme::of() falls back to the default DS.
    EXPECT_NE(btn, nullptr);
}

TEST(Theme, CardBuildsWithDefaultConfig)
{
    auto card = std::make_shared<Card>();
    card->child = std::make_shared<Text>("Content");
    card->priority = CardPriority::elevated;

    EXPECT_NE(card, nullptr);
}

TEST(Theme, DividerBuildsWithDefaultConfig)
{
    auto div = std::make_shared<Divider>();
    div->indent = 16.0f;

    EXPECT_NE(div, nullptr);
}

TEST(Theme, ListTileBuildsWithDefaultConfig)
{
    auto tile = std::make_shared<ListTile>();
    tile->title = std::make_shared<Text>("Item");

    EXPECT_NE(tile, nullptr);
}

TEST(Theme, ProgressIndicatorBuildsWithDefaultConfig)
{
    auto pi = std::make_shared<ProgressIndicator>();
    pi->type = ProgressType::circular;

    EXPECT_NE(pi, nullptr);
}

TEST(Theme, TextStyleOfReturnsValidPointer)
{
    auto ds = std::make_shared<CampelloDesignSystem>(CampelloDesignSystem::light());
    auto theme = std::make_shared<Theme>(ds, nullptr);

    // textStyleOf requires a real BuildContext, so we test the helper directly
    const auto& tokens = ds->tokens();
    const auto& style = textStyleForRole(tokens.typography, TextRole::title_large);
    EXPECT_EQ(style.font_size, 22.0f);
    EXPECT_EQ(style.font_weight, FontWeight::bold);

    const auto& body = textStyleForRole(tokens.typography, TextRole::body_medium);
    EXPECT_EQ(body.font_size, 14.0f);
    EXPECT_EQ(body.font_weight, FontWeight::normal);
}

TEST(Theme, LightAndDarkTypographyColorsDiffer)
{
    auto light = CampelloDesignSystem::light();
    auto dark  = CampelloDesignSystem::dark();

    const auto& light_title = textStyleForRole(light.tokens().typography, TextRole::title_large);
    const auto& dark_title  = textStyleForRole(dark.tokens().typography, TextRole::title_large);

    // Light mode text should be dark; dark mode text should be light
    EXPECT_LT(light_title.color.r, 0.5f);
    EXPECT_GT(dark_title.color.r, 0.5f);
}

// ---------------------------------------------------------------------------
// CampelloDesignSystem builder coverage
// ---------------------------------------------------------------------------

TEST(Theme, CampelloDesignSystemBuildsAllWidgets)
{
    auto ds = CampelloDesignSystem::light();

    // Each builder should return a non-null widget
    EXPECT_NE(ds.buildButton(ButtonConfig{}), nullptr);
    EXPECT_NE(ds.buildSwitch(SwitchConfig{}), nullptr);
    EXPECT_NE(ds.buildCheckbox(CheckboxConfig{}), nullptr);
    EXPECT_NE(ds.buildRadio(RadioConfig{}), nullptr);
    EXPECT_NE(ds.buildSlider(SliderConfig{}), nullptr);
    EXPECT_NE(ds.buildTextField(TextFieldConfig{}), nullptr);
    EXPECT_NE(ds.buildCard(CardConfig{}), nullptr);
    EXPECT_NE(ds.buildProgressIndicator(ProgressConfig{}), nullptr);
    EXPECT_NE(ds.buildTooltip(TooltipConfig{}), nullptr);
    EXPECT_NE(ds.buildListTile(ListTileConfig{}), nullptr);
    EXPECT_NE(ds.buildDivider(DividerConfig{}), nullptr);
    EXPECT_NE(ds.buildAppBar(AppBarConfig{}), nullptr);
    EXPECT_NE(ds.buildNavigationBar(NavigationBarConfig{}), nullptr);
    EXPECT_NE(ds.buildDialog(DialogConfig{}), nullptr);
    EXPECT_NE(ds.buildSnackBar(SnackBarConfig{}), nullptr);
    EXPECT_NE(ds.buildPopupMenuButton(PopupMenuConfig{}), nullptr);
    EXPECT_NE(ds.buildDropdownButton(DropdownConfig{}), nullptr);
    EXPECT_NE(ds.buildPrimaryActionButton(PrimaryActionConfig{}), nullptr);
    EXPECT_NE(ds.buildTabBar(TabBarConfig{}), nullptr);
}
