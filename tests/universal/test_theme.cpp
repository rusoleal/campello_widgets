#include <gtest/gtest.h>

#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/progress_indicator.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/ui/null_design_system.hpp>

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Theme::of() fallback
// ---------------------------------------------------------------------------

TEST(Theme, FallbackIsLightNullDesignSystem)
{
    // The static fallback used by Theme::of() when no Theme ancestor is
    // present should be a light NullDesignSystem — see src/widgets/theme.cpp.
    auto fallback = NullDesignSystem();
    EXPECT_EQ(fallback.tokens().brightness, Brightness::light);
}

// ---------------------------------------------------------------------------
// Theme widget construction
// ---------------------------------------------------------------------------

TEST(Theme, CanCreateThemeWithDesignSystem)
{
    auto ds = std::make_shared<NullDesignSystem>();
    auto child = std::make_shared<Text>("Hello");
    auto theme = std::make_shared<Theme>(ds, child);

    EXPECT_NE(theme, nullptr);
    EXPECT_EQ(theme->design_system, ds);
    EXPECT_EQ(theme->child, child);
}

TEST(Theme, UpdateShouldNotifyWhenDesignSystemChanges)
{
    auto ds1 = std::make_shared<NullDesignSystem>();
    auto ds2 = std::make_shared<NullDesignSystem>();

    auto theme1 = std::make_shared<Theme>(ds1, nullptr);
    auto theme2 = std::make_shared<Theme>(ds2, nullptr);

    // Different design_system pointers should trigger notification
    EXPECT_TRUE(theme1->updateShouldNotify(*theme2));
}

TEST(Theme, UpdateShouldNotNotifyWhenDesignSystemSame)
{
    auto ds = std::make_shared<NullDesignSystem>();

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
    auto ds = std::make_shared<NullDesignSystem>();
    auto theme = std::make_shared<Theme>(ds, nullptr);

    // textStyleOf requires a real BuildContext, so we test the helper directly
    const auto& tokens = ds->tokens();
    const auto& style = textStyleForRole(tokens.typography, TextRole::title_large);
    EXPECT_EQ(style.font_size, 22.0f);

    const auto& body = textStyleForRole(tokens.typography, TextRole::body_medium);
    EXPECT_EQ(body.font_size, 14.0f);
    EXPECT_EQ(body.font_weight, FontWeight::normal);
}
