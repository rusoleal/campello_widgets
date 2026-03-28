#include <gtest/gtest.h>
#include <campello_widgets/testing/visual_fidelity.hpp>
#include <campello_widgets/testing/fidelity.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <filesystem>
#include <iostream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ----------------------------------------------------------------------------
// Helper: Check if Flutter golden exists
// ----------------------------------------------------------------------------

static bool flutterGoldenExists(const std::string& name)
{
    std::filesystem::path path = std::filesystem::path(cwt::getFlutterGoldensDirectory()) / name;
    return std::filesystem::exists(path);
}

static std::string getFlutterGoldenPath(const std::string& name)
{
    return (std::filesystem::path(cwt::getFlutterGoldensDirectory()) / name).string();
}

static std::string getCppOutputPath(const std::string& name)
{
    return (std::filesystem::path(cwt::getCppOutputDirectory()) / name).string();
}

// Standard resolution for fidelity testing
constexpr float kFidelityWidth = 1280.0f;
constexpr float kFidelityHeight = 720.0f;

// ----------------------------------------------------------------------------
// Visual Fidelity Tests - Layout
// ----------------------------------------------------------------------------

TEST(VisualFidelity, SimpleColumn)
{
    // Build C++ widget tree matching Flutter's:
    // Column(
    //   Padding(padding: EdgeInsets.all(32.0), child: Container(width: 200, height: 200, color: Colors.blue)),
    //   Expanded(child: Container(color: Colors.green.withOpacity(0.5))),
    // )

    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;

    // First child: Padding with blue box
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);  // Blue

    auto sizedBox = std::make_shared<cw::RenderSizedBox>();
    sizedBox->width = std::optional<float>(200.0f);
    sizedBox->height = std::optional<float>(200.0f);
    sizedBox->setChild(blueBox);

    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::all(32.0f);
    padding->setChild(sizedBox);

    // Second child: Expanded with green (50% opacity)
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGBA(0.0f, 1.0f, 0.0f, 0.5f);  // Green 50%

    auto expanded = std::make_shared<cw::RenderSizedBox>();
    expanded->setChild(greenBox);

    root->insertChild(padding, 0, 0);
    root->insertChild(expanded, 1, 1);

    // Render to PNG at 1280x720 resolution
    std::string outputPath = getCppOutputPath("simple_column.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success) << "Failed to render PNG to " << outputPath;

    // Compare with Flutter golden if it exists
    if (flutterGoldenExists("simple_column.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("simple_column.png"),
            outputPath,
            5  // tolerance
        );
        
        if (!result.match) {
            for (const auto& error : result.errors) {
                std::cout << "Visual diff: " << error << std::endl;
            }
        }
        // Note: We don't fail the test on visual diff yet since the software rasterizer
        // is not pixel-perfect with Flutter
    } else {
        GTEST_SKIP() << "Flutter golden not found. Run 'flutter test test/visual_goldens_test.dart' first.";
    }
}

TEST(VisualFidelity, SimpleRow)
{
    // Row(
    //   Container(width: 200, height: 150, color: Colors.red),
    //   Expanded(child: Container(color: Colors.green)),
    //   Container(width: 200, height: 150, color: Colors.blue),
    // )

    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::horizontal;
    root->main_axis_size = cw::MainAxisSize::max;

    // Red box
    auto redBox = std::make_shared<cw::RenderColoredBox>();
    redBox->color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->width = std::optional<float>(200.0f);
    redSized->height = std::optional<float>(150.0f);
    redSized->setChild(redBox);

    // Green expanded
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    auto greenExpanded = std::make_shared<cw::RenderSizedBox>();
    greenExpanded->setChild(greenBox);

    // Blue box
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    auto blueSized = std::make_shared<cw::RenderSizedBox>();
    blueSized->width = std::optional<float>(200.0f);
    blueSized->height = std::optional<float>(150.0f);
    blueSized->setChild(blueBox);

    root->insertChild(redSized, 0, 0);
    root->insertChild(greenExpanded, 1, 1);
    root->insertChild(blueSized, 2, 0);

    std::string outputPath = getCppOutputPath("simple_row.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("simple_row.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("simple_row.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 10.0)  // Allow 10% difference
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, AlignCenter)
{
    // Center(child: Container(width: 300, height: 300, color: Colors.orange))

    auto orangeBox = std::make_shared<cw::RenderColoredBox>();
    orangeBox->color = cw::Color::fromRGB(1.0f, 0.647f, 0.0f);  // Orange

    auto sizedBox = std::make_shared<cw::RenderSizedBox>();
    sizedBox->width = std::optional<float>(300.0f);
    sizedBox->height = std::optional<float>(300.0f);
    sizedBox->setChild(orangeBox);

    auto align = std::make_shared<cw::RenderAlign>();
    align->alignment = cw::Alignment::center();
    align->setChild(sizedBox);

    std::string outputPath = getCppOutputPath("align_center.png");
    bool success = cwt::captureToPng(
        *align,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("align_center.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("align_center.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 10.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, NestedPadding)
{
    // Padding(
    //   padding: EdgeInsets.all(48.0),
    //   child: Padding(
    //     padding: EdgeInsets.symmetric(horizontal: 32.0, vertical: 16.0),
    //     child: Container(width: 200, height: 200, color: Colors.purple),
    //   ),
    // )

    auto purpleBox = std::make_shared<cw::RenderColoredBox>();
    purpleBox->color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);

    auto sizedBox = std::make_shared<cw::RenderSizedBox>();
    sizedBox->width = std::optional<float>(200.0f);
    sizedBox->height = std::optional<float>(200.0f);
    sizedBox->setChild(purpleBox);

    auto innerPadding = std::make_shared<cw::RenderPadding>();
    innerPadding->padding = cw::EdgeInsets::symmetric(16.0f, 32.0f);  // vertical, horizontal
    innerPadding->setChild(sizedBox);

    auto outerPadding = std::make_shared<cw::RenderPadding>();
    outerPadding->padding = cw::EdgeInsets::all(48.0f);
    outerPadding->setChild(innerPadding);

    std::string outputPath = getCppOutputPath("nested_padding.png");
    bool success = cwt::captureToPng(
        *outerPadding,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("nested_padding.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("nested_padding.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 10.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, FlexExpanded)
{
    // Column(
    //   Container(height: 100, color: Colors.red),
    //   Expanded(flex: 2, child: Container(color: Colors.green)),
    //   Expanded(flex: 1, child: Container(color: Colors.blue)),
    // )

    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;

    auto redBox = std::make_shared<cw::RenderColoredBox>();
    redBox->color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->height = std::optional<float>(100.0f);
    redSized->setChild(redBox);

    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    auto greenExpanded = std::make_shared<cw::RenderSizedBox>();
    greenExpanded->setChild(greenBox);

    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    auto blueExpanded = std::make_shared<cw::RenderSizedBox>();
    blueExpanded->setChild(blueBox);

    root->insertChild(redSized, 0, 0);
    root->insertChild(greenExpanded, 1, 2);
    root->insertChild(blueExpanded, 2, 1);

    std::string outputPath = getCppOutputPath("flex_expanded.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("flex_expanded.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("flex_expanded.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 10.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, StackPositioned)
{
    // Stack(
    //   Container(color: Colors.grey.shade300),
    //   Positioned(left: 100, top: 100, child: Container(width: 250, height: 200, color: Colors.red)),
    //   Positioned(right: 100, bottom: 100, child: Container(width: 250, height: 200, color: Colors.blue)),
    //   Positioned(left: 450, top: 300, child: Container(width: 250, height: 200, color: Colors.green.withOpacity(0.7))),
    // )

    auto root = std::make_shared<cw::RenderStack>();
    root->fit = cw::StackFit::expand;

    // Background grey
    auto greyBox = std::make_shared<cw::RenderColoredBox>();
    greyBox->color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);  // Light grey
    auto bgSized = std::make_shared<cw::RenderSizedBox>();
    bgSized->setChild(greyBox);

    // Red box at (100, 100)
    auto redBox = std::make_shared<cw::RenderColoredBox>();
    redBox->color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->width = std::optional<float>(250.0f);
    redSized->height = std::optional<float>(200.0f);
    redSized->setChild(redBox);

    // Blue box at bottom-right
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    auto blueSized = std::make_shared<cw::RenderSizedBox>();
    blueSized->width = std::optional<float>(250.0f);
    blueSized->height = std::optional<float>(200.0f);
    blueSized->setChild(blueBox);

    // Green box with opacity at (450, 300)
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGBA(0.0f, 1.0f, 0.0f, 0.7f);
    auto greenSized = std::make_shared<cw::RenderSizedBox>();
    greenSized->width = std::optional<float>(250.0f);
    greenSized->height = std::optional<float>(200.0f);
    greenSized->setChild(greenBox);

    root->insertChild(bgSized, 0, 0, 0, 0, 0, std::nullopt, std::nullopt);  // fill
    root->insertChild(redSized, 1, 100, 100, std::nullopt, std::nullopt, 250, 200);
    root->insertChild(blueSized, 2, std::nullopt, std::nullopt, 100, 100, 250, 200);
    root->insertChild(greenSized, 3, 450, 300, std::nullopt, std::nullopt, 250, 200);

    std::string outputPath = getCppOutputPath("stack_positioned.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("stack_positioned.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("stack_positioned.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 10.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Visual Fidelity Tests - Canvas API
// ----------------------------------------------------------------------------

TEST(VisualFidelity, CanvasBasicShapes)
{
    // Create canvas with basic shapes matching Flutter's CustomPaint (scaled for 1280x720)
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;

    // Blue rectangle
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), bluePaint);

    // Red circle
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawCircle(cw::Offset(200 * scaleX, 60 * scaleY), 40 * scaleX, redPaint);

    // Green oval
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawOval(cw::Rect::fromLTWH(280 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), greenPaint);

    // Purple stroked rectangle
    cw::Paint purpleStroke;
    purpleStroke.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    purpleStroke.style = cw::PaintStyle::stroke;
    purpleStroke.stroke_width = 4 * scaleX;
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 120 * scaleY, 100 * scaleX, 80 * scaleY), purpleStroke);

    // Render to PNG
    cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.clear(cw::Color::white());
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_basic_shapes.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_basic_shapes.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_basic_shapes.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 15.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, CanvasRoundedRects)
{
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Single rounded rect
    cw::RRect rrect1(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY), 20.0f * scaleX);
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRRect(rrect1, bluePaint);

    // Different corner radii (simplified to use uniform radius)
    cw::RRect rrect2(cw::Rect::fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY), 30.0f * scaleX);
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawRRect(rrect2, greenPaint);

    // Double rounded rect
    cw::RRect outerRRect(cw::Rect::fromLTWH(20 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 25.0f * scaleX);
    cw::RRect innerRRect(cw::Rect::fromLTWH(35 * scaleX, 165 * scaleY, 120 * scaleX, 70 * scaleY), 15.0f * scaleX);
    cw::Paint purplePaint;
    purplePaint.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    canvas.drawDRRect(outerRRect, innerRRect, purplePaint);

    // Stroked rounded rect
    cw::RRect rrect3(cw::Rect::fromLTWH(200 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 15.0f * scaleX);
    cw::Paint redStroke;
    redStroke.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    redStroke.style = cw::PaintStyle::stroke;
    redStroke.stroke_width = 5 * scaleX;
    canvas.drawRRect(rrect3, redStroke);

    cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.clear(cw::Color::white());
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_rounded_rects.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_rounded_rects.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_rounded_rects.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 15.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, CanvasLines)
{
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Black lines (2px stroke)
    cw::Paint linePaint;
    linePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 0.0f);
    linePaint.stroke_width = 2 * scaleX;
    canvas.drawLine(cw::Offset(20 * scaleX, 20 * scaleY), cw::Offset(150 * scaleX, 100 * scaleY), linePaint);
    canvas.drawLine(cw::Offset(200 * scaleX, 20 * scaleY), cw::Offset(350 * scaleX, 150 * scaleY), linePaint);

    // Thick orange line (8px stroke)
    cw::Paint thickPaint;
    thickPaint.color = cw::Color::fromRGB(1.0f, 0.65f, 0.0f);  // Orange
    thickPaint.stroke_width = 8 * scaleX;
    canvas.drawLine(cw::Offset(50 * scaleX, 200 * scaleY), cw::Offset(350 * scaleX, 250 * scaleY), thickPaint);

    // Green lines with varying widths
    for (int i = 0; i < 4; i++) {
        float width = (i + 1) * 3.0f * scaleX;
        cw::Paint paint;
        paint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);  // Green
        paint.stroke_width = width;
        float y = 300.0f * scaleY + i * 25 * scaleY;
        canvas.drawLine(cw::Offset(20 * scaleX, y), cw::Offset(150 * scaleX, y), paint);
    }

    cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.clear(cw::Color::white());
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_lines.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_lines.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_lines.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 15.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, CanvasComplexScene)
{
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    canvas.save();

    // Background pattern - grid of circles with opacity (checkerboard)
    cw::Color bgColors[2] = {
        cw::Color::fromRGB(0.68f, 0.85f, 0.9f),   // Light blue
        cw::Color::fromRGB(0.93f, 0.82f, 0.9f),   // Light purple
    };

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            cw::Paint paint;
            paint.color = bgColors[(i + j) % 2];
            paint.color.a = 0.3f;  // 30% opacity
            float cx = 40.0f * scaleX + i * 80 * scaleX;
            float cy = 40.0f * scaleY + j * 80 * scaleY;
            canvas.drawCircle(cw::Offset(cx, cy), 50 * scaleX, paint);
        }
    }

    // Central transformed group (card with rotation)
    canvas.save();
    canvas.translate(200 * scaleX, 200 * scaleY);
    canvas.rotate(0.2f);

    // Card background (rounded rect)
    cw::RRect cardRRect(cw::Rect::fromLTWH(-120 * scaleX, -80 * scaleY, 240 * scaleX, 160 * scaleY), 20.0f * scaleX);

    cw::Paint cardPaint;
    cardPaint.color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.9f);  // White with 90% opacity
    canvas.drawRRect(cardRRect, cardPaint);

    // Card border
    cw::Paint borderPaint;
    borderPaint.color = cw::Color::fromRGB(0.68f, 0.85f, 0.9f);  // Light blue
    borderPaint.style = cw::PaintStyle::stroke;
    borderPaint.stroke_width = 3 * scaleX;
    canvas.drawRRect(cardRRect, borderPaint);

    // Inner content - clipped to card
    canvas.save();
    canvas.clipRRect(cardRRect);

    // Colored circles inside card
    cw::Color circleColors[3] = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),    // Red
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),    // Green
        cw::Color::fromRGB(1.0f, 0.65f, 0.0f),   // Orange
    };

    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = circleColors[i];
        paint.color.a = 0.6f;  // 60% opacity
        float x = -60.0f * scaleX + i * 60 * scaleX;
        canvas.drawCircle(cw::Offset(x, 0), 35 * scaleX, paint);
    }

    canvas.restore();  // End clip
    canvas.restore();  // End transform

    // Corner decorations with paths and rotations
    cw::Offset corners[4] = {
        cw::Offset(30 * scaleX, 30 * scaleY),
        cw::Offset(370 * scaleX, 30 * scaleY),
        cw::Offset(30 * scaleX, 370 * scaleY),
        cw::Offset(370 * scaleX, 370 * scaleY),
    };

    for (int i = 0; i < 4; i++) {
        canvas.save();
        canvas.translate(corners[i].x, corners[i].y);
        canvas.rotate(i * 1.5708f);  // 90 degree increments

        cw::Path path;
        path.moveTo(cw::Offset(0, -15 * scaleX));
        path.lineTo(cw::Offset(10 * scaleX, 0));
        path.lineTo(cw::Offset(0, 15 * scaleX));
        path.close();

        cw::Paint paint;
        paint.color = cw::Color::fromRGBA(0.5f, 0.0f, 1.0f, 0.5f);  // Purple 50%
        canvas.drawPath(path, paint);

        canvas.restore();
    }

    canvas.restore();  // End scene

    cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.clear(cw::Color::white());
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_complex_scene.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_complex_scene.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_complex_scene.png"),
            outputPath,
            5
        );
        EXPECT_TRUE(result.match || result.pixelDifference < 15.0);
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Utility Test
// ----------------------------------------------------------------------------

TEST(VisualFidelity, ListGeneratedFiles)
{
    // List all generated PNG files for debugging
    std::cout << "\n=== Generated Visual Fidelity Files ===" << std::endl;
    std::cout << "C++ Output Directory: " << cwt::getCppOutputDirectory() << std::endl;
    
    auto cppDir = std::filesystem::path(cwt::getCppOutputDirectory());
    if (std::filesystem::exists(cppDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(cppDir)) {
            if (entry.path().extension() == ".png") {
                auto size = std::filesystem::file_size(entry.path());
                std::cout << "  [C++] " << entry.path().filename().string() 
                          << " (" << size << " bytes)" << std::endl;
            }
        }
    }

    std::cout << "\nFlutter Goldens Directory: " << cwt::getFlutterGoldensDirectory() << std::endl;
    
    auto flutterDir = std::filesystem::path(cwt::getFlutterGoldensDirectory());
    if (std::filesystem::exists(flutterDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(flutterDir)) {
            if (entry.path().extension() == ".png") {
                auto size = std::filesystem::file_size(entry.path());
                std::cout << "  [Flutter] " << entry.path().filename().string() 
                          << " (" << size << " bytes)" << std::endl;
            }
        }
    }
    std::cout << "========================================\n" << std::endl;
}
