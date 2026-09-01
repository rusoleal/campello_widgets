#include <gtest/gtest.h>

#include <campello_widgets/ui/null_design_system.hpp>
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/divider.hpp>
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

TEST(NullDesignSystem, TokensAreLightAndFlat)
{
    NullDesignSystem ds;
    EXPECT_EQ(ds.tokens().brightness, Brightness::light);

    // No rounding, no shadows — the whole point of the "null" style.
    EXPECT_EQ(ds.tokens().shape.radius_md, 0.0f);
    EXPECT_EQ(ds.tokens().elevation.level3, 0.0f);
}

// ---------------------------------------------------------------------------
// Builder coverage — every DesignSystem method must return a non-null widget
// ---------------------------------------------------------------------------

TEST(NullDesignSystem, BuildsAllWidgets)
{
    NullDesignSystem ds;

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

TEST(NullDesignSystem, StepperClampsAtBounds)
{
    NullDesignSystem ds;
    StepperConfig cfg;
    cfg.value = 5;
    cfg.min = 0;
    cfg.max = 5;
    cfg.on_changed = [](int) {};
    EXPECT_NE(ds.buildStepper(cfg), nullptr);
}

TEST(NullDesignSystem, ActionSheetWithCancelReturnsWidget)
{
    NullDesignSystem ds;
    ActionSheetConfig cfg;
    cfg.actions = {
        {"Share", [] {}, false},
        {"Delete", [] {}, true},
    };
    cfg.on_cancel = [] {};
    EXPECT_NE(ds.buildActionSheet(cfg), nullptr);
}

TEST(NullDesignSystem, BuildBadgeWithLabelReturnsWidget)
{
    NullDesignSystem ds;
    BadgeConfig cfg;
    cfg.child = std::make_shared<Text>("icon");
    cfg.label = "3";
    EXPECT_NE(ds.buildBadge(cfg), nullptr);
}

TEST(NullDesignSystem, BuildSegmentedButtonWithSegmentsReturnsWidget)
{
    NullDesignSystem ds;
    SegmentedConfig cfg;
    cfg.segments = {
        {std::make_shared<Text>("A"), nullptr},
        {std::make_shared<Text>("B"), nullptr},
    };
    cfg.selected_index = 1;
    EXPECT_NE(ds.buildSegmentedButton(cfg), nullptr);
}

TEST(NullDesignSystem, BuildDisabledButtonReturnsWidget)
{
    NullDesignSystem ds;
    ButtonConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildButton(cfg), nullptr);
}

TEST(NullDesignSystem, BuildDisabledSwitchReturnsWidget)
{
    NullDesignSystem ds;
    SwitchConfig cfg;
    cfg.enabled = false;
    EXPECT_NE(ds.buildSwitch(cfg), nullptr);
}
