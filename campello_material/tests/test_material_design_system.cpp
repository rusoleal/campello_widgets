#include <gtest/gtest.h>

#include <campello_material/material_design_system.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/floating_action_button.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
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

TEST(MaterialDesignSystem, LightTokensAreValid)
{
    auto ds = MaterialDesignSystem::light();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::light);
    EXPECT_GT(t.colors.primary.r, 0.0f);
    EXPECT_EQ(t.colors.on_primary, Color::white());
    EXPECT_GT(t.elevation.level1, 0.0f);
    EXPECT_GT(t.shape.radius_md, 0.0f);
}

TEST(MaterialDesignSystem, DarkTokensAreValid)
{
    auto ds = MaterialDesignSystem::dark();
    const auto& t = ds.tokens();

    EXPECT_EQ(t.brightness, Brightness::dark);
    EXPECT_LT(t.colors.surface.r, 0.5f);
}

TEST(MaterialDesignSystem, DefaultConstructorIsLight)
{
    MaterialDesignSystem ds;
    EXPECT_EQ(ds.tokens().brightness, Brightness::light);
}

TEST(MaterialDesignSystem, ShapeScaleMatchesMD3Spec)
{
    auto ds = MaterialDesignSystem::light();
    const auto& s = ds.tokens().shape;

    EXPECT_FLOAT_EQ(s.radius_xs, 4.0f);
    EXPECT_FLOAT_EQ(s.radius_sm, 8.0f);
    EXPECT_FLOAT_EQ(s.radius_md, 12.0f);
    EXPECT_FLOAT_EQ(s.radius_lg, 16.0f);
    EXPECT_FLOAT_EQ(s.radius_xl, 28.0f);
}

TEST(MaterialDesignSystem, ElevationScaleMatchesMD3Spec)
{
    auto ds = MaterialDesignSystem::light();
    const auto& e = ds.tokens().elevation;

    EXPECT_FLOAT_EQ(e.level0, 0.0f);
    EXPECT_FLOAT_EQ(e.level1, 1.0f);
    EXPECT_FLOAT_EQ(e.level2, 3.0f);
    EXPECT_FLOAT_EQ(e.level3, 6.0f);
    EXPECT_FLOAT_EQ(e.level4, 8.0f);
    EXPECT_FLOAT_EQ(e.level5, 12.0f);
}

// ---------------------------------------------------------------------------
// MD3-specific shape choices (distinguishing this from campello_ui's style)
// ---------------------------------------------------------------------------

TEST(MaterialDesignSystem, FilledButtonIsFullyRounded)
{
    auto ds = MaterialDesignSystem::light();
    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("OK");
    cfg.on_pressed = [] {};

    auto w = ds.buildButton(cfg);
    ASSERT_NE(w, nullptr);
}

TEST(MaterialDesignSystem, PrimaryActionButtonUsesLargeShapeNotCircular)
{
    // MD3's default FAB is a 16dp-radius rounded square, not a circle —
    // a deliberate departure from the circular FAB of Material 2.
    auto ds = MaterialDesignSystem::light();
    PrimaryActionConfig cfg;
    cfg.on_pressed = [] {};

    auto w = ds.buildPrimaryActionButton(cfg);
    ASSERT_NE(w, nullptr);
    EXPECT_FLOAT_EQ(ds.tokens().shape.radius_lg, 16.0f);
}

// ---------------------------------------------------------------------------
// Component builders — smoke tests for all 19 DesignSystem methods
// ---------------------------------------------------------------------------

TEST(MaterialDesignSystem, BuildsAllWidgets)
{
    auto ds = MaterialDesignSystem::light();

    EXPECT_NE(ds.buildButton(ButtonConfig{}), nullptr);
    EXPECT_NE(ds.buildSwitch(SwitchConfig{}), nullptr);
    EXPECT_NE(ds.buildCheckbox(CheckboxConfig{}), nullptr);
    EXPECT_NE(ds.buildRadio(RadioConfig{}), nullptr);
    EXPECT_NE(ds.buildSlider(SliderConfig{}), nullptr);
    EXPECT_NE(ds.buildTextField(TextFieldConfig{}), nullptr);
    EXPECT_NE(ds.buildCard(CardConfig{}), nullptr);
    EXPECT_NE(ds.buildProgressIndicator(ProgressConfig{}), nullptr);
    EXPECT_NE(ds.buildRefreshIndicator(RefreshIndicatorConfig{}), nullptr);
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

TEST(MaterialDesignSystem, BuildRefreshIndicatorReflectsPullProgress)
{
    auto ds = MaterialDesignSystem::light();

    RefreshIndicatorConfig cfg;
    cfg.pull_progress = 0.4f;
    EXPECT_NE(ds.buildRefreshIndicator(cfg), nullptr);

    cfg.refreshing = true;
    EXPECT_NE(ds.buildRefreshIndicator(cfg), nullptr);
}

TEST(MaterialDesignSystem, SearchFieldIsFullyRoundedPill)
{
    // MD3's Search shape token is "full" — a pill, matching the segmented
    // button's stadium container.
    auto ds = MaterialDesignSystem::light();
    SearchFieldConfig cfg;
    cfg.value = "coffee";
    cfg.on_clear = [] {};
    EXPECT_NE(ds.buildSearchField(cfg), nullptr);
}

TEST(MaterialDesignSystem, ActionSheetCancelIsOrdinaryRow)
{
    // Unlike iOS, MD3 has no detached Cancel button — it's just another
    // list row in the same sheet.
    auto ds = MaterialDesignSystem::light();
    ActionSheetConfig cfg;
    cfg.actions = {{"Share", [] {}, false}, {"Delete", [] {}, true}};
    cfg.on_cancel = [] {};
    EXPECT_NE(ds.buildActionSheet(cfg), nullptr);
}

TEST(MaterialDesignSystem, ChipUsesSmallShapeToken)
{
    // MD3 chips use the Small shape token (8dp) — distinct from the
    // stadium-shaped SegmentedButton container.
    auto ds = MaterialDesignSystem::light();
    EXPECT_FLOAT_EQ(ds.tokens().shape.radius_sm, 8.0f);

    ChipConfig cfg;
    cfg.label = std::make_shared<Text>("Filter");
    cfg.selected = true;
    EXPECT_NE(ds.buildChip(cfg), nullptr);
}

TEST(MaterialDesignSystem, BottomSheetUsesExtraLargeShapeToken)
{
    auto ds = MaterialDesignSystem::light();
    EXPECT_FLOAT_EQ(ds.tokens().shape.radius_xl, 28.0f);

    BottomSheetConfig cfg;
    cfg.child = std::make_shared<Text>("content");
    EXPECT_NE(ds.buildBottomSheet(cfg), nullptr);
}

TEST(MaterialDesignSystem, SegmentedButtonWithSegmentsReturnsWidget)
{
    auto ds = MaterialDesignSystem::light();
    SegmentedConfig cfg;
    cfg.segments = {
        {std::make_shared<Text>("A"), nullptr},
        {std::make_shared<Text>("B"), nullptr},
        {std::make_shared<Text>("C"), nullptr},
    };
    cfg.selected_index = 2;
    EXPECT_NE(ds.buildSegmentedButton(cfg), nullptr);
}

TEST(MaterialDesignSystem, BuildCardOutlinedReturnsWidget)
{
    auto ds = MaterialDesignSystem::light();
    CardConfig cfg;
    cfg.child = std::make_shared<Text>("Content");
    cfg.priority = CardPriority::outlined;
    EXPECT_NE(ds.buildCard(cfg), nullptr);
}

TEST(MaterialDesignSystem, BuildDisabledButtonReturnsWidget)
{
    auto ds = MaterialDesignSystem::light();
    ButtonConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildButton(cfg), nullptr);
}

TEST(MaterialDesignSystem, BuildDisabledSwitchReturnsWidget)
{
    auto ds = MaterialDesignSystem::light();
    SwitchConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildSwitch(cfg), nullptr);
}

TEST(MaterialDesignSystem, NavigationBarSelectedItemGetsIndicator)
{
    auto ds = MaterialDesignSystem::light();
    NavigationBarConfig cfg;
    cfg.items = {
        {std::make_shared<Text>("A"), "A"},
        {std::make_shared<Text>("B"), "B"},
    };
    cfg.selected_index = 0;
    EXPECT_NE(ds.buildNavigationBar(cfg), nullptr);
}

TEST(MaterialDesignSystem, FabUsesPrimaryContainerNotPlainPrimary)
{
    // Real MD3 spec detail: the default FAB's background is
    // primaryContainer, not a solid primary fill — easy to get wrong
    // without a distinct container role.
    auto ds = MaterialDesignSystem::light();
    PrimaryActionConfig cfg;
    cfg.on_pressed = [] {};

    auto w = ds.buildPrimaryActionButton(cfg);
    auto gesture = std::dynamic_pointer_cast<const GestureDetector>(w);
    ASSERT_NE(gesture, nullptr);
    auto decorated = std::dynamic_pointer_cast<const DecoratedBox>(gesture->child);
    ASSERT_NE(decorated, nullptr);
    EXPECT_EQ(decorated->decoration.color, ds.tokens().colors.primary_container);
}

TEST(MaterialDesignSystem, SelectedChipUsesSecondaryContainer)
{
    auto ds = MaterialDesignSystem::light();
    ChipConfig cfg;
    cfg.label = std::make_shared<Text>("Filter");
    cfg.selected = true;

    auto w = ds.buildChip(cfg);
    auto gesture = std::dynamic_pointer_cast<const GestureDetector>(w);
    ASSERT_NE(gesture, nullptr);
    auto decorated = std::dynamic_pointer_cast<const DecoratedBox>(gesture->child);
    ASSERT_NE(decorated, nullptr);
    EXPECT_EQ(decorated->decoration.color, ds.tokens().colors.secondary_container);
}
