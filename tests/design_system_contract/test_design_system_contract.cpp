// Parameterized abstract-contract suite: runs identical assertions against
// every concrete DesignSystem implementation via the abstract base pointer.
// Complements (does not replace) each library's own style-specific tests —
// this file only asserts things that must hold for ANY DesignSystem,
// regardless of visual style.

#include <gtest/gtest.h>

#include <campello_widgets/ui/null_design_system.hpp>
#include <campello_ui/campello_design_system.hpp>
#include <campello_material/material_design_system.hpp>
#include <campello_cupertino/cupertino_design_system.hpp>

#include <campello_widgets/widgets/text.hpp>

using namespace systems::leal::campello_widgets;

namespace
{
    enum class Kind
    {
        null_ds,
        campello_ui,
        material,
        cupertino,
    };

    std::unique_ptr<DesignSystem> makeDesignSystem(Kind kind)
    {
        switch (kind) {
            case Kind::campello_ui:
                return std::make_unique<CampelloDesignSystem>(CampelloDesignSystem::light());
            case Kind::material:
                return std::make_unique<MaterialDesignSystem>(MaterialDesignSystem::light());
            case Kind::cupertino:
                return std::make_unique<CupertinoDesignSystem>(CupertinoDesignSystem::light());
            case Kind::null_ds:
            default:
                return std::make_unique<NullDesignSystem>();
        }
    }

    std::string kindName(const ::testing::TestParamInfo<Kind>& info)
    {
        switch (info.param) {
            case Kind::campello_ui: return "CampelloUi";
            case Kind::material:    return "Material";
            case Kind::cupertino:   return "Cupertino";
            case Kind::null_ds:
            default:                return "Null";
        }
    }
} // namespace

class DesignSystemContractTest : public ::testing::TestWithParam<Kind>
{
protected:
    std::unique_ptr<DesignSystem> ds;
    void SetUp() override { ds = makeDesignSystem(GetParam()); }
};

// ---------------------------------------------------------------------------
// Every builder returns a widget for a default-constructed config
// ---------------------------------------------------------------------------

TEST_P(DesignSystemContractTest, AllBuildersReturnNonNullForDefaultConfig)
{
    EXPECT_NE(ds->buildButton(ButtonConfig{}), nullptr);
    EXPECT_NE(ds->buildSwitch(SwitchConfig{}), nullptr);
    EXPECT_NE(ds->buildCheckbox(CheckboxConfig{}), nullptr);
    EXPECT_NE(ds->buildRadio(RadioConfig{}), nullptr);
    EXPECT_NE(ds->buildSlider(SliderConfig{}), nullptr);
    EXPECT_NE(ds->buildTextField(TextFieldConfig{}), nullptr);
    EXPECT_NE(ds->buildCard(CardConfig{}), nullptr);
    EXPECT_NE(ds->buildProgressIndicator(ProgressConfig{}), nullptr);
    EXPECT_NE(ds->buildRefreshIndicator(RefreshIndicatorConfig{}), nullptr);
    EXPECT_NE(ds->buildTooltip(TooltipConfig{}), nullptr);
    EXPECT_NE(ds->buildListTile(ListTileConfig{}), nullptr);
    EXPECT_NE(ds->buildDivider(DividerConfig{}), nullptr);
    EXPECT_NE(ds->buildAppBar(AppBarConfig{}), nullptr);
    EXPECT_NE(ds->buildNavigationBar(NavigationBarConfig{}), nullptr);
    EXPECT_NE(ds->buildDialog(DialogConfig{}), nullptr);
    EXPECT_NE(ds->buildSnackBar(SnackBarConfig{}), nullptr);
    EXPECT_NE(ds->buildPopupMenuButton(PopupMenuConfig{}), nullptr);
    EXPECT_NE(ds->buildDropdownButton(DropdownConfig{}), nullptr);
    EXPECT_NE(ds->buildPrimaryActionButton(PrimaryActionConfig{}), nullptr);
    EXPECT_NE(ds->buildTabBar(TabBarConfig{}), nullptr);
    EXPECT_NE(ds->buildChip(ChipConfig{}), nullptr);
    EXPECT_NE(ds->buildSegmentedButton(SegmentedConfig{}), nullptr);
    EXPECT_NE(ds->buildBottomSheet(BottomSheetConfig{}), nullptr);
    EXPECT_NE(ds->buildBadge(BadgeConfig{}), nullptr);
    EXPECT_NE(ds->buildIconButton(IconButtonConfig{}), nullptr);
    EXPECT_NE(ds->buildStepper(StepperConfig{}), nullptr);
    EXPECT_NE(ds->buildRatingIndicator(RatingConfig{}), nullptr);
    EXPECT_NE(ds->buildActionSheet(ActionSheetConfig{}), nullptr);
    EXPECT_NE(ds->buildSearchField(SearchFieldConfig{}), nullptr);
    EXPECT_NE(ds->buildDatePicker(DatePickerConfig{}), nullptr);
    EXPECT_NE(ds->buildTimePicker(TimePickerConfig{}), nullptr);
    EXPECT_NE(ds->buildExpansionTile(ExpansionTileConfig{}), nullptr);
    EXPECT_NE(ds->buildToggleButtons(ToggleButtonsConfig{}), nullptr);
    EXPECT_NE(ds->buildBanner(BannerConfig{}), nullptr);
    EXPECT_NE(ds->buildNavigationRail(NavigationRailConfig{}), nullptr);
    EXPECT_NE(ds->buildDataTable(DataTableConfig{}), nullptr);
}

// ---------------------------------------------------------------------------
// Disabled variants must still return a widget (never null just because
// the control is inert)
// ---------------------------------------------------------------------------

TEST_P(DesignSystemContractTest, DisabledControlsStillReturnWidgets)
{
    ButtonConfig btn; btn.enabled = false;
    EXPECT_NE(ds->buildButton(btn), nullptr);

    SwitchConfig sw; sw.enabled = false;
    EXPECT_NE(ds->buildSwitch(sw), nullptr);

    ChipConfig chip; chip.enabled = false;
    EXPECT_NE(ds->buildChip(chip), nullptr);

    IconButtonConfig icon_btn; icon_btn.enabled = false;
    EXPECT_NE(ds->buildIconButton(icon_btn), nullptr);

    StepperConfig stepper; stepper.enabled = false;
    EXPECT_NE(ds->buildStepper(stepper), nullptr);

    ExpansionTileConfig tile; tile.enabled = false;
    EXPECT_NE(ds->buildExpansionTile(tile), nullptr);

    ToggleButtonsConfig toggles; toggles.enabled = false;
    EXPECT_NE(ds->buildToggleButtons(toggles), nullptr);
}

// ---------------------------------------------------------------------------
// Config content is honored structurally (labels/content get through)
// ---------------------------------------------------------------------------

TEST_P(DesignSystemContractTest, ButtonWithLabelAndCallbackBuildsWidget)
{
    ButtonConfig cfg;
    cfg.label = std::make_shared<Text>("OK");
    cfg.on_pressed = [] {};
    EXPECT_NE(ds->buildButton(cfg), nullptr);
}

TEST_P(DesignSystemContractTest, ActionSheetWithActionsAndCancelBuildsWidget)
{
    ActionSheetConfig cfg;
    cfg.actions = {{"Share", [] {}, false}, {"Delete", [] {}, true}};
    cfg.on_cancel = [] {};
    EXPECT_NE(ds->buildActionSheet(cfg), nullptr);
}

TEST_P(DesignSystemContractTest, ExpansionTileExpandedWithChildrenContentBuildsWidget)
{
    ExpansionTileConfig cfg;
    cfg.title = std::make_shared<Text>("Section");
    cfg.expanded = true;
    cfg.children_content = std::make_shared<Text>("Body");
    cfg.on_expansion_changed = [](bool) {};
    EXPECT_NE(ds->buildExpansionTile(cfg), nullptr);
}

TEST_P(DesignSystemContractTest, ToggleButtonsWithMultipleItemsBuildsWidget)
{
    ToggleButtonsConfig cfg;
    cfg.items = {
        {std::make_shared<Text>("A"), nullptr, true},
        {std::make_shared<Text>("B"), nullptr, false},
    };
    cfg.on_pressed = [](int) {};
    EXPECT_NE(ds->buildToggleButtons(cfg), nullptr);
}

TEST_P(DesignSystemContractTest, NavigationRailWithItemsBuildsWidget)
{
    NavigationRailConfig cfg;
    cfg.items = {{nullptr, "Home"}, {nullptr, "Search"}};
    cfg.selected_index = 1;
    cfg.on_tap = [](int) {};
    EXPECT_NE(ds->buildNavigationRail(cfg), nullptr);
}

TEST_P(DesignSystemContractTest, DataTableWithColumnsAndRowsBuildsWidget)
{
    DataTableConfig cfg;
    cfg.columns = {"Name", "Value"};
    cfg.rows = {
        {std::make_shared<Text>("A"), std::make_shared<Text>("1")},
        {std::make_shared<Text>("B"), std::make_shared<Text>("2")},
    };
    EXPECT_NE(ds->buildDataTable(cfg), nullptr);
}

// ---------------------------------------------------------------------------
// Token internal consistency — must hold regardless of visual style
// ---------------------------------------------------------------------------

TEST_P(DesignSystemContractTest, TokensReportLightBrightnessByDefault)
{
    EXPECT_EQ(ds->tokens().brightness, Brightness::light);
}

TEST_P(DesignSystemContractTest, ShapeScaleIsNonDecreasing)
{
    const auto& s = ds->tokens().shape;
    EXPECT_LE(s.radius_xs, s.radius_sm);
    EXPECT_LE(s.radius_sm, s.radius_md);
    EXPECT_LE(s.radius_md, s.radius_lg);
    EXPECT_LE(s.radius_lg, s.radius_xl);
    EXPECT_LE(s.radius_xl, s.radius_full);
}

TEST_P(DesignSystemContractTest, ElevationScaleIsNonDecreasing)
{
    const auto& e = ds->tokens().elevation;
    EXPECT_LE(e.level0, e.level1);
    EXPECT_LE(e.level1, e.level2);
    EXPECT_LE(e.level2, e.level3);
    EXPECT_LE(e.level3, e.level4);
    EXPECT_LE(e.level4, e.level5);
}

TEST_P(DesignSystemContractTest, TypographyScaleDescendsAcrossTiers)
{
    const auto& t = ds->tokens().typography;
    EXPECT_GE(t.display_large.font_size, t.headline_large.font_size);
    EXPECT_GE(t.headline_large.font_size, t.title_large.font_size);
    EXPECT_GE(t.title_large.font_size, t.body_large.font_size);
    EXPECT_GE(t.body_large.font_size, t.label_large.font_size);
    EXPECT_GT(t.label_small.font_size, 0.0f);
}

TEST_P(DesignSystemContractTest, SpacingScaleIsPositiveAndNonDecreasing)
{
    const auto& s = ds->tokens().spacing;
    EXPECT_GT(s.xs, 0.0f);
    EXPECT_LE(s.xs, s.sm);
    EXPECT_LE(s.sm, s.md);
    EXPECT_LE(s.md, s.lg);
    EXPECT_LE(s.lg, s.xl);
    EXPECT_LE(s.xl, s.xxl);
    EXPECT_LE(s.xxl, s.xxxl);
}

TEST_P(DesignSystemContractTest, OnColorsDifferFromTheirBaseColor)
{
    // "on_X" exists specifically to be legible against X — if they were
    // ever equal, text/icons drawn on that surface would be invisible.
    const auto& c = ds->tokens().colors;
    EXPECT_NE(c.primary, c.on_primary);
    EXPECT_NE(c.secondary, c.on_secondary);
    EXPECT_NE(c.surface, c.on_surface);
    EXPECT_NE(c.error, c.on_error);
}

TEST_P(DesignSystemContractTest, TertiaryAndContainerOnColorsDifferFromTheirBaseColor)
{
    const auto& c = ds->tokens().colors;
    EXPECT_NE(c.tertiary, c.on_tertiary);
    EXPECT_NE(c.primary_container, c.on_primary_container);
    EXPECT_NE(c.secondary_container, c.on_secondary_container);
    EXPECT_NE(c.tertiary_container, c.on_tertiary_container);
    EXPECT_NE(c.error_container, c.on_error_container);
}

TEST_P(DesignSystemContractTest, ContainerColorsAreDistinctFromTheirBaseRole)
{
    // A "container" role exists to be a visually distinct tonal fill from
    // its base role (e.g. a selected chip's primary_container background
    // shouldn't be identical to a solid primary-filled button) — if they
    // collapsed to the same color, the container role would be pointless.
    const auto& c = ds->tokens().colors;
    EXPECT_NE(c.primary, c.primary_container);
    EXPECT_NE(c.secondary, c.secondary_container);
    EXPECT_NE(c.tertiary, c.tertiary_container);
    EXPECT_NE(c.error, c.error_container);
}

// ---------------------------------------------------------------------------

INSTANTIATE_TEST_SUITE_P(
    AllDesignSystems,
    DesignSystemContractTest,
    ::testing::Values(Kind::null_ds, Kind::campello_ui, Kind::material, Kind::cupertino),
    kindName);
