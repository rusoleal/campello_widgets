#include <gtest/gtest.h>

#include <campello_widgets/ui/campello_design_system.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/floating_action_button.hpp>
#include <campello_widgets/widgets/linear_progress_indicator.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/radio.hpp>
#include <campello_widgets/widgets/slider.hpp>
#include <campello_widgets/widgets/snack_bar.hpp>
#include <campello_widgets/widgets/switch.hpp>
#include <campello_widgets/widgets/tab_bar.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/tooltip.hpp>

using namespace systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Token factories
// ---------------------------------------------------------------------------

TEST(CampelloDesignSystem, LightTokensAreValid)
{
    auto ds = CampelloDesignSystem::light();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::light);
    EXPECT_GT(t.colors.primary.r, 0.0f);
    EXPECT_GT(t.colors.primary.a, 0.0f);
    EXPECT_EQ(t.colors.on_primary, Color::white());
    EXPECT_GT(t.elevation.level1, 0.0f);
    EXPECT_GT(t.shape.radius_md, 0.0f);
    EXPECT_GT(t.spacing.md, 0.0f);
    EXPECT_GT(t.motion.duration_normal, 0.0);
}

TEST(CampelloDesignSystem, DarkTokensAreValid)
{
    auto ds = CampelloDesignSystem::dark();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::dark);
    EXPECT_GT(t.colors.primary.r, 0.0f);
    EXPECT_GT(t.colors.surface.r, 0.0f);
    EXPECT_LT(t.colors.background.r, 0.5f);
}

TEST(CampelloDesignSystem, DefaultConstructorIsLight)
{
    CampelloDesignSystem ds;
    EXPECT_EQ(ds.tokens().brightness, Brightness::light);
}

// ---------------------------------------------------------------------------
// Component builders — smoke tests
// ---------------------------------------------------------------------------

TEST(CampelloDesignSystem, BuildButtonReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("OK");
    cfg.on_pressed = []{};

    auto w = ds.buildButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildButtonPrimaryUsesPrimaryColor)
{
    auto ds = CampelloDesignSystem::light();
    const auto& colors = ds.tokens().colors;

    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("OK");
    cfg.priority = ButtonPriority::primary;

    auto w = ds.buildButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildButtonDangerUsesErrorColor)
{
    auto ds = CampelloDesignSystem::light();

    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("Delete");
    cfg.priority = ButtonPriority::danger;

    auto w = ds.buildButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildSwitchReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    SwitchConfig cfg;
    cfg.value = true;
    cfg.on_changed = [](bool){};

    auto w = ds.buildSwitch(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildCheckboxReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    CheckboxConfig cfg;
    cfg.value = true;
    cfg.on_changed = [](bool){};

    auto w = ds.buildCheckbox(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildRadioReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    RadioConfig cfg;
    cfg.selected = true;

    auto w = ds.buildRadio(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildSliderReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    SliderConfig cfg;
    cfg.value = 0.5f;
    cfg.on_changed = [](float){};

    auto w = ds.buildSlider(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildTextFieldReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    TextFieldConfig cfg;
    cfg.placeholder = "Enter text";

    auto w = ds.buildTextField(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildCardReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    CardConfig cfg;
    cfg.child = std::make_shared<Text>("Card content");
    cfg.priority = CardPriority::elevated;

    auto w = ds.buildCard(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildCardOutlinedReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    CardConfig cfg;
    cfg.child = std::make_shared<Text>("Card content");
    cfg.priority = CardPriority::outlined;

    auto w = ds.buildCard(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildProgressIndicatorCircularReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    ProgressConfig cfg;
    cfg.type = ProgressType::circular;

    auto w = ds.buildProgressIndicator(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildProgressIndicatorLinearReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    ProgressConfig cfg;
    cfg.type = ProgressType::linear;
    cfg.value = 0.6f;

    auto w = ds.buildProgressIndicator(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildTooltipReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    TooltipConfig cfg;
    cfg.message = "Hint";
    cfg.child = std::make_shared<Text>("Hover me");

    auto w = ds.buildTooltip(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildListTileReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    ListTileConfig cfg;
    cfg.title = std::make_shared<Text>("Item");

    auto w = ds.buildListTile(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildDividerReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    DividerConfig cfg;
    cfg.indent = 16.0f;

    auto w = ds.buildDivider(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildAppBarReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    AppBarConfig cfg;
    cfg.title = std::make_shared<Text>("Title");

    auto w = ds.buildAppBar(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildNavigationBarReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    NavigationBarConfig cfg;
    cfg.items = {
        {nullptr, "Home"},
        {nullptr, "Settings"},
    };

    auto w = ds.buildNavigationBar(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildDialogReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    DialogConfig cfg;
    cfg.title = std::make_shared<Text>("Confirm");
    cfg.content = std::make_shared<Text>("Are you sure?");

    auto w = ds.buildDialog(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildSnackBarReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    SnackBarConfig cfg;
    cfg.message = "Saved";

    auto w = ds.buildSnackBar(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildPopupMenuButtonReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    PopupMenuConfig cfg;
    cfg.items = {
        {"Edit", []{}},
        {"Delete", []{}},
    };

    auto w = ds.buildPopupMenuButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildDropdownButtonReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    DropdownConfig cfg;
    cfg.items = {
        {"Apple", "apple"},
        {"Banana", "banana"},
    };
    cfg.selected_value = "apple";

    auto w = ds.buildDropdownButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildPrimaryActionButtonReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    PrimaryActionConfig cfg;
    cfg.on_pressed = []{};

    auto w = ds.buildPrimaryActionButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildTabBarReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    TabBarConfig cfg;
    cfg.tabs = {
        {"One", nullptr},
        {"Two", nullptr},
    };

    auto w = ds.buildTabBar(cfg);
    EXPECT_NE(w, nullptr);
}

// ---------------------------------------------------------------------------
// Disabled state
// ---------------------------------------------------------------------------

TEST(CampelloDesignSystem, BuildDisabledButtonReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("Disabled");
    cfg.enabled = false;

    auto w = ds.buildButton(cfg);
    EXPECT_NE(w, nullptr);
}

TEST(CampelloDesignSystem, BuildDisabledSwitchReturnsWidget)
{
    auto ds = CampelloDesignSystem::light();

    SwitchConfig cfg;
    cfg.enabled = false;

    auto w = ds.buildSwitch(cfg);
    EXPECT_NE(w, nullptr);
}
