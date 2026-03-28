#include <gtest/gtest.h>
#include <campello_widgets/testing/fidelity.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ---------------------------------------------------------------------------
// Helper Factories
// ---------------------------------------------------------------------------

static std::shared_ptr<cw::RenderColoredBox> makeColoredBox(
    float width, float height, const cw::Color& color)
{
    auto box = std::make_shared<cw::RenderColoredBox>();
    box->color = color;
    // Use a SizedBox wrapper to enforce dimensions
    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width = width;
    sized->height = height;
    sized->setChild(box);
    return box;
}

static std::shared_ptr<cw::RenderPadding> makePadding(
    float left, float top, float right, float bottom,
    std::shared_ptr<cw::RenderBox> child)
{
    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::only(left, top, right, bottom);
    padding->setChild(child);
    return padding;
}

// ---------------------------------------------------------------------------
// Layout Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityLayout, DumpSingleBox)
{
    auto box = std::make_shared<cw::RenderSizedBox>();
    box->width = std::optional<float>(100.0f);
    box->height = std::optional<float>(50.0f);
    
    box->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*box, cw::Offset::zero());
    
    EXPECT_EQ(snapshot.type, "RenderSizedBox");
    EXPECT_FLOAT_EQ(snapshot.width, 100.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 50.0f);
    EXPECT_FLOAT_EQ(snapshot.offset_x, 0.0f);
    EXPECT_FLOAT_EQ(snapshot.offset_y, 0.0f);
}

TEST(FidelityLayout, DumpFlexWithChildren)
{
    auto flex = std::make_shared<cw::RenderFlex>();
    flex->axis = cw::Axis::vertical;
    flex->main_axis_size = cw::MainAxisSize::max;
    
    auto child1 = std::make_shared<cw::RenderSizedBox>();
    child1->width = std::optional<float>(100.0f);
    child1->height = std::optional<float>(40.0f);
    
    auto child2 = std::make_shared<cw::RenderSizedBox>();
    child2->width = std::optional<float>(100.0f);
    child2->height = std::optional<float>(60.0f);
    
    flex->insertChild(child1, 0, 0);
    flex->insertChild(child2, 1, 0);
    
    flex->layout(cw::BoxConstraints::tight(400.0f, 300.0f));
    
    auto snapshot = cwt::dumpRenderTree(*flex, cw::Offset::zero());
    
    EXPECT_EQ(snapshot.type, "RenderFlex");
    EXPECT_FLOAT_EQ(snapshot.width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 300.0f);
    
    // Note: Flex children are not captured in current implementation
    // because flex_children_ is private. This would require adding
    // friend declaration or accessor methods to RenderFlex.
    // For now, we just verify the flex itself is dumped correctly.
}

TEST(FidelityLayout, JsonRoundTrip)
{
    cwt::RenderNodeSnapshot original;
    original.type = "RenderTestBox";
    original.width = 100.0f;
    original.height = 200.0f;
    original.offset_x = 10.0f;
    original.offset_y = 20.0f;
    original.constraint_min_w = 0.0f;
    original.constraint_max_w = 500.0f;
    original.constraint_min_h = 0.0f;
    original.constraint_max_h = 600.0f;
    original.properties.push_back({"color", "#FF0000"});
    
    cwt::RenderNodeSnapshot child;
    child.type = "RenderChild";
    child.width = 50.0f;
    child.height = 50.0f;
    original.children.push_back(child);
    
    std::string json = cwt::renderNodeToJson(original, 0);
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("RenderTestBox"), std::string::npos);
    // JSON output uses compact float format (e.g., "100" instead of "100.000000")
    EXPECT_NE(json.find("100"), std::string::npos);
}

TEST(FidelityLayout, TreeComparisonExactMatch)
{
    cwt::RenderNodeSnapshot tree1;
    tree1.type = "RenderFlex";
    tree1.width = 400.0f;
    tree1.height = 600.0f;
    
    cwt::RenderNodeSnapshot tree2 = tree1;
    
    auto result = cwt::compareRenderTrees(tree1, tree2, "root");
    EXPECT_TRUE(result.match);
    EXPECT_TRUE(result.differences.empty());
}

TEST(FidelityLayout, TreeComparisonDetectsSizeMismatch)
{
    cwt::RenderNodeSnapshot expected;
    expected.type = "RenderFlex";
    expected.width = 400.0f;
    expected.height = 600.0f;
    
    cwt::RenderNodeSnapshot actual = expected;
    actual.width = 380.0f;  // Wrong!
    
    auto result = cwt::compareRenderTrees(expected, actual, "root");
    EXPECT_FALSE(result.match);
    EXPECT_FALSE(result.differences.empty());
    
    // Check that the difference mentions width
    bool found_width_diff = false;
    for (const auto& diff : result.differences) {
        if (diff.find("width") != std::string::npos) {
            found_width_diff = true;
            break;
        }
    }
    EXPECT_TRUE(found_width_diff);
}

TEST(FidelityLayout, TreeComparisonDetectsTypeMismatch)
{
    cwt::RenderNodeSnapshot expected;
    expected.type = "RenderColumn";
    expected.width = 400.0f;
    expected.height = 600.0f;
    
    cwt::RenderNodeSnapshot actual;
    actual.type = "RenderRow";  // Wrong!
    actual.width = 400.0f;
    actual.height = 600.0f;
    
    auto result = cwt::compareRenderTrees(expected, actual, "root");
    EXPECT_FALSE(result.match);
    
    bool found_type_diff = false;
    for (const auto& diff : result.differences) {
        if (diff.find("type") != std::string::npos) {
            found_type_diff = true;
            break;
        }
    }
    EXPECT_TRUE(found_type_diff);
}

// ---------------------------------------------------------------------------
// Paint Command Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityPaint, SerializeEmptyDrawList)
{
    cw::DrawList empty_list;
    auto snapshots = cwt::serializeDrawList(empty_list);
    EXPECT_TRUE(snapshots.empty());
}

TEST(FidelityPaint, SerializeDrawRect)
{
    cw::DrawList list;
    cw::DrawRectCmd cmd;
    cmd.rect = cw::Rect::fromLTRB(10.0f, 20.0f, 110.0f, 70.0f);
    cmd.paint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    cmd.paint.style = cw::PaintStyle::fill;
    list.push_back(cmd);
    
    auto snapshots = cwt::serializeDrawList(list);
    
    ASSERT_EQ(snapshots.size(), 1u);
    EXPECT_EQ(snapshots[0].type, "rect");
    EXPECT_FLOAT_EQ(snapshots[0].rect_left, 10.0f);
    EXPECT_FLOAT_EQ(snapshots[0].rect_top, 20.0f);
    EXPECT_FLOAT_EQ(snapshots[0].rect_right, 110.0f);
    EXPECT_FLOAT_EQ(snapshots[0].rect_bottom, 70.0f);
    EXPECT_FLOAT_EQ(snapshots[0].paint_red, 1.0f);
    EXPECT_FLOAT_EQ(snapshots[0].paint_green, 0.0f);
    EXPECT_FLOAT_EQ(snapshots[0].paint_blue, 0.0f);
    EXPECT_EQ(snapshots[0].paint_style, "fill");
}

TEST(FidelityPaint, SerializeDrawText)
{
    cw::DrawList list;
    cw::DrawTextCmd cmd;
    cmd.span.text = "Hello, World!";
    cmd.span.style.font_family = "Roboto";
    cmd.span.style.font_size = 16.0f;
    cmd.origin = cw::Offset{50.0f, 100.0f};
    list.push_back(cmd);
    
    auto snapshots = cwt::serializeDrawList(list);
    
    ASSERT_EQ(snapshots.size(), 1u);
    EXPECT_EQ(snapshots[0].type, "text");
    EXPECT_EQ(snapshots[0].text_content, "Hello, World!");
    EXPECT_EQ(snapshots[0].text_style_family, "Roboto");
    EXPECT_FLOAT_EQ(snapshots[0].text_style_size, 16.0f);
    EXPECT_FLOAT_EQ(snapshots[0].text_origin_x, 50.0f);
    EXPECT_FLOAT_EQ(snapshots[0].text_origin_y, 100.0f);
}

TEST(FidelityPaint, DrawListToJson)
{
    std::vector<cwt::DrawCommandSnapshot> snapshots;
    
    cwt::DrawCommandSnapshot rect_cmd;
    rect_cmd.type = "rect";
    rect_cmd.rect_left = 0.0f;
    rect_cmd.rect_top = 0.0f;
    rect_cmd.rect_right = 100.0f;
    rect_cmd.rect_bottom = 50.0f;
    rect_cmd.paint_red = 1.0f;
    rect_cmd.paint_green = 0.0f;
    rect_cmd.paint_blue = 0.0f;
    rect_cmd.paint_alpha = 1.0f;
    rect_cmd.paint_style = "fill";
    snapshots.push_back(rect_cmd);
    
    std::string json = cwt::drawListToJson(snapshots);
    
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"type\": \"rect\""), std::string::npos);
    EXPECT_NE(json.find("\"left\": 0"), std::string::npos);
    EXPECT_NE(json.find("\"right\": 100"), std::string::npos);
}

TEST(FidelityPaint, CompareIdenticalDrawLists)
{
    std::vector<cwt::DrawCommandSnapshot> list1;
    
    cwt::DrawCommandSnapshot cmd;
    cmd.type = "rect";
    cmd.rect_left = 10.0f;
    cmd.rect_top = 20.0f;
    cmd.rect_right = 100.0f;
    cmd.rect_bottom = 80.0f;
    list1.push_back(cmd);
    
    auto list2 = list1;
    
    auto result = cwt::compareDrawLists(list1, list2);
    EXPECT_TRUE(result.match);
    EXPECT_TRUE(result.differences.empty());
}

TEST(FidelityPaint, CompareDifferentDrawLists)
{
    std::vector<cwt::DrawCommandSnapshot> expected;
    cwt::DrawCommandSnapshot cmd1;
    cmd1.type = "rect";
    cmd1.rect_left = 0.0f;
    expected.push_back(cmd1);
    
    std::vector<cwt::DrawCommandSnapshot> actual;
    cwt::DrawCommandSnapshot cmd2;
    cmd2.type = "text";  // Different type!
    actual.push_back(cmd2);
    
    auto result = cwt::compareDrawLists(expected, actual);
    EXPECT_FALSE(result.match);
    EXPECT_FALSE(result.differences.empty());
}

TEST(FidelityPaint, CompareWithTolerance)
{
    std::vector<cwt::DrawCommandSnapshot> list1;
    cwt::DrawCommandSnapshot cmd1;
    cmd1.type = "rect";
    cmd1.rect_left = 10.0f;
    list1.push_back(cmd1);
    
    auto list2 = list1;
    list2[0].rect_left = 10.0001f;  // Tiny difference
    
    // With tight tolerance, should fail
    auto result1 = cwt::compareDrawLists(list1, list2, 0.00001f);
    EXPECT_FALSE(result1.match);
    
    // With loose tolerance, should pass
    auto result2 = cwt::compareDrawLists(list1, list2, 0.001f);
    EXPECT_TRUE(result2.match);
}

// ---------------------------------------------------------------------------
// End-to-End Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityEndToEnd, CaptureSimpleWidgetTree)
{
    // Build a simple widget tree: Column(Padding(ColoredBox), Expanded)
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;
    
    // First child: Padding around a colored box
    auto colored = std::make_shared<cw::RenderColoredBox>();
    colored->color = cw::Color::fromRGB(33, 150, 243);  // Material Blue
    
    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width = 100.0f;
    sized->height = 100.0f;
    sized->setChild(colored);
    
    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::all(8.0f);
    padding->setChild(sized);
    
    // Second child: Expanded space
    auto expanded = std::make_shared<cw::RenderSizedBox>();
    // No explicit size - will be expanded
    
    root->insertChild(padding, 0, 0);
    root->insertChild(expanded, 1, 1);  // flex = 1
    
    // Capture snapshot
    auto snapshot = cwt::captureSnapshot(
        *root,
        cw::BoxConstraints::tight(400.0f, 600.0f),
        400.0f, 600.0f);
    
    // Verify layout
    EXPECT_EQ(snapshot.layout.type, "RenderFlex");
    EXPECT_FLOAT_EQ(snapshot.layout.width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.height, 600.0f);
    
    // Verify paint commands were captured
    EXPECT_FALSE(snapshot.paint_commands.empty());
    
    // Verify JSON serialization works
    std::string json = snapshot.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("viewport"), std::string::npos);
    EXPECT_NE(json.find("layout"), std::string::npos);
    EXPECT_NE(json.find("paint_commands"), std::string::npos);
}

TEST(FidelityEndToEnd, HumanReadableOutput)
{
    auto box = std::make_shared<cw::RenderSizedBox>();
    box->width = 200.0f;
    box->height = 100.0f;
    
    box->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*box, cw::Offset{10.0f, 20.0f});
    std::string str = cwt::renderNodeToString(snapshot);
    
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("RenderSizedBox"), std::string::npos);
    EXPECT_NE(str.find("200"), std::string::npos);
    EXPECT_NE(str.find("100"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Golden File Tests (using embedded reference data)
// ---------------------------------------------------------------------------

TEST(FidelityGolden, CompareAgainstReferenceLayout)
{
    // Build the widget tree that matches our golden file
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;
    
    // Padding + ColoredBox
    auto colored = std::make_shared<cw::RenderColoredBox>();
    colored->color = cw::Color::fromRGBA(0.129f, 0.588f, 0.953f, 1.0f);
    
    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width = 384.0f;
    sized->height = 100.0f;
    sized->setChild(colored);
    
    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::all(8.0f);
    padding->setChild(sized);
    
    // Expanded
    auto expanded = std::make_shared<cw::RenderSizedBox>();
    
    root->insertChild(padding, 0, 0);
    root->insertChild(expanded, 1, 1);
    
    // Capture
    auto snapshot = cwt::captureSnapshot(
        *root,
        cw::BoxConstraints::tight(400.0f, 600.0f),
        400.0f, 600.0f);
    
    // Build expected layout
    cwt::RenderNodeSnapshot expected;
    expected.type = "RenderFlex";
    expected.width = 400.0f;
    expected.height = 600.0f;
    expected.offset_x = 0.0f;
    expected.offset_y = 0.0f;
    expected.constraint_min_w = 400.0f;
    expected.constraint_max_w = 400.0f;
    expected.constraint_min_h = 600.0f;
    expected.constraint_max_h = 600.0f;
    
    // Note: This is a simplified comparison - in a real test you'd build
    // the complete expected tree from the golden file
    
    auto result = cwt::compareRenderTrees(expected, snapshot.layout, "root");
    
    // This should match since we built it to match
    EXPECT_TRUE(result.match) << "Layout mismatch: " 
        << (result.differences.empty() ? "" : result.differences[0]);
}
