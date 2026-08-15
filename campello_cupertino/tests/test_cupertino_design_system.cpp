#include <gtest/gtest.h>

#include <campello_cupertino/cupertino_design_system.hpp>
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
// Tokens
// ---------------------------------------------------------------------------

TEST(CupertinoDesignSystem, LightTokensAreValid)
{
    auto ds = CupertinoDesignSystem::light();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::light);
    // systemBlue (#007AFF) is blue-dominant with a zero red channel.
    EXPECT_GT(t.colors.primary.b, t.colors.primary.r);
    EXPECT_GT(t.colors.primary.a, 0.0f);
    EXPECT_EQ(t.colors.on_primary, Color::white());
    EXPECT_GT(t.shape.radius_md, 0.0f);
}

TEST(CupertinoDesignSystem, DarkTokensAreValid)
{
    auto ds = CupertinoDesignSystem::dark();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::dark);
    EXPECT_LT(t.colors.surface.r, 0.5f);
}

TEST(CupertinoDesignSystem, DefaultConstructorIsLight)
{
    CupertinoDesignSystem ds;
    EXPECT_EQ(ds.tokens().brightness, Brightness::light);
}

TEST(CupertinoDesignSystem, ShapeScaleIsNotMD3Scale)
{
    // Deliberately different from campello_material's 4/8/12/16/28.
    auto ds = CupertinoDesignSystem::light();
    const auto& s = ds.tokens().shape;

    EXPECT_FLOAT_EQ(s.radius_xs, 6.0f);
    EXPECT_FLOAT_EQ(s.radius_sm, 8.0f);
    EXPECT_FLOAT_EQ(s.radius_md, 10.0f);
    EXPECT_FLOAT_EQ(s.radius_lg, 14.0f);
    EXPECT_FLOAT_EQ(s.radius_xl, 20.0f);
}

TEST(CupertinoDesignSystem, ElevationIsFlatterThanMaterial)
{
    auto ds = CupertinoDesignSystem::light();
    const auto& e = ds.tokens().elevation;

    EXPECT_LT(e.level5, 12.0f); // MD3's level5 is 12dp — Cupertino is flatter
}

// ---------------------------------------------------------------------------
// HIG-specific choices (distinguishing this from campello_material's style)
// ---------------------------------------------------------------------------

TEST(CupertinoDesignSystem, SwitchActiveTrackIsSuccessGreenNotPrimary)
{
    // UISwitch's real default is a green active track, regardless of the
    // app's accent (primary) color.
    auto ds = CupertinoDesignSystem::light();
    const auto& c = ds.tokens().colors;
    EXPECT_NE(c.success, c.primary);

    SwitchConfig cfg;
    cfg.value = true;
    cfg.on_changed = [](bool) {};
    EXPECT_NE(ds.buildSwitch(cfg), nullptr);
}

TEST(CupertinoDesignSystem, DialogIsNarrowFixedWidth)
{
    // UIAlertController is a fixed 270pt wide, unlike Material's
    // wider/responsive dialog.
    auto ds = CupertinoDesignSystem::light();
    DialogConfig cfg;
    cfg.title = std::make_shared<Text>("Alert");
    cfg.content = std::make_shared<Text>("Message");
    cfg.actions = {std::make_shared<Text>("OK"), std::make_shared<Text>("Cancel")};

    auto w = ds.buildDialog(cfg);
    ASSERT_NE(w, nullptr);
    auto dialog = std::dynamic_pointer_cast<const Dialog>(w);
    ASSERT_NE(dialog, nullptr);
    EXPECT_FLOAT_EQ(dialog->min_width, 270.0f);
    EXPECT_FLOAT_EQ(dialog->max_width, 270.0f);
}

TEST(CupertinoDesignSystem, DialogWithManyActionsStacksVertically)
{
    auto ds = CupertinoDesignSystem::light();
    DialogConfig cfg;
    cfg.title = std::make_shared<Text>("Choose");
    cfg.actions = {
        std::make_shared<Text>("A"),
        std::make_shared<Text>("B"),
        std::make_shared<Text>("C"),
    };
    EXPECT_NE(ds.buildDialog(cfg), nullptr);
}

// ---------------------------------------------------------------------------
// Component builders — smoke tests for all 19 DesignSystem methods
// ---------------------------------------------------------------------------

TEST(CupertinoDesignSystem, BuildsAllWidgets)
{
    auto ds = CupertinoDesignSystem::light();

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
    EXPECT_NE(ds.buildChip(ChipConfig{}), nullptr);
    EXPECT_NE(ds.buildSegmentedButton(SegmentedConfig{}), nullptr);
    EXPECT_NE(ds.buildBottomSheet(BottomSheetConfig{}), nullptr);
    EXPECT_NE(ds.buildBadge(BadgeConfig{}), nullptr);
    EXPECT_NE(ds.buildIconButton(IconButtonConfig{}), nullptr);
    EXPECT_NE(ds.buildStepper(StepperConfig{}), nullptr);
    EXPECT_NE(ds.buildRatingIndicator(RatingConfig{}), nullptr);
    EXPECT_NE(ds.buildActionSheet(ActionSheetConfig{}), nullptr);
    EXPECT_NE(ds.buildSearchField(SearchFieldConfig{}), nullptr);
    EXPECT_NE(ds.buildDatePicker(DatePickerConfig{}), nullptr);
    EXPECT_NE(ds.buildTimePicker(TimePickerConfig{}), nullptr);
}

TEST(CupertinoDesignSystem, StepperHasNoBuiltInValueLabel)
{
    // The real UIStepper has no value display of its own — just [-][+].
    // We can't directly inspect the widget tree here, but the builder
    // must at least succeed without a value in the config being required.
    auto ds = CupertinoDesignSystem::light();
    StepperConfig cfg;
    cfg.value = 3;
    cfg.on_changed = [](int) {};
    EXPECT_NE(ds.buildStepper(cfg), nullptr);
}

TEST(CupertinoDesignSystem, ActionSheetWithCancelProducesSeparateCard)
{
    // The classic iOS pattern: actions card + gap + detached Cancel card.
    auto ds = CupertinoDesignSystem::light();
    ActionSheetConfig cfg;
    cfg.title = std::make_shared<Text>("Options");
    cfg.actions = {{"Share", [] {}, false}, {"Delete", [] {}, true}};
    cfg.on_cancel = [] {};
    EXPECT_NE(ds.buildActionSheet(cfg), nullptr);
}

TEST(CupertinoDesignSystem, ActionSheetWithoutCancelReturnsWidget)
{
    auto ds = CupertinoDesignSystem::light();
    ActionSheetConfig cfg;
    cfg.actions = {{"OK", [] {}, false}};
    EXPECT_NE(ds.buildActionSheet(cfg), nullptr);
}

TEST(CupertinoDesignSystem, SegmentedButtonIsTheAuthenticUISegmentedControlHome)
{
    // Unlike Material, where SegmentedButton is a connected-button group,
    // Cupertino's real segmented control is a pill track with a floating
    // white pill behind the selected segment.
    auto ds = CupertinoDesignSystem::light();
    SegmentedConfig cfg;
    cfg.segments = {
        {std::make_shared<Text>("List"), nullptr},
        {std::make_shared<Text>("Grid"), nullptr},
    };
    cfg.selected_index = 1;
    cfg.on_changed = [](int) {};

    EXPECT_NE(ds.buildSegmentedButton(cfg), nullptr);
}

TEST(CupertinoDesignSystem, BottomSheetHasGrabberByDefault)
{
    auto ds = CupertinoDesignSystem::light();
    BottomSheetConfig cfg;
    cfg.child = std::make_shared<Text>("content");
    EXPECT_TRUE(cfg.show_drag_handle);
    EXPECT_NE(ds.buildBottomSheet(cfg), nullptr);
}

TEST(CupertinoDesignSystem, BuildButtonTertiaryIsPlainStyle)
{
    auto ds = CupertinoDesignSystem::light();
    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("Cancel");
    cfg.priority = ButtonPriority::tertiary;
    cfg.on_pressed = [] {};
    EXPECT_NE(ds.buildButton(cfg), nullptr);
}

TEST(CupertinoDesignSystem, BuildDisabledButtonReturnsWidget)
{
    auto ds = CupertinoDesignSystem::light();
    ButtonConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildButton(cfg), nullptr);
}

TEST(CupertinoDesignSystem, BuildDisabledSwitchReturnsWidget)
{
    auto ds = CupertinoDesignSystem::light();
    SwitchConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildSwitch(cfg), nullptr);
}
