#include <gtest/gtest.h>
#include "visual_fidelity.hpp"
#include "gpu_visual_renderer.hpp"
#include "fidelity.hpp"
#include "visual_fidelity_helpers.hpp"
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/render_text.hpp>
#include <campello_widgets/ui/render_opacity.hpp>
#include <campello_widgets/ui/render_transform.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <filesystem>
#include <iostream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

using cwt::kFidelityWidth;
using cwt::kFidelityHeight;
using cwt::flutterGoldenExists;
using cwt::getFlutterGoldenPath;
using cwt::getCppOutputPath;

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
    root->cross_axis_alignment = cw::CrossAxisAlignment::center;

    // First child: Padding with blue box
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Colors.blue

    auto sizedBox = std::make_shared<cw::RenderSizedBox>();
    sizedBox->width = std::optional<float>(200.0f);
    sizedBox->height = std::optional<float>(200.0f);
    sizedBox->setChild(blueBox);

    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::all(32.0f);
    padding->setChild(sizedBox);

    // Second child: Expanded with green (50% opacity)
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGBA(0.2980f, 0.6863f, 0.3137f, 0.5020f);  // Colors.green.withAlpha(128)

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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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
    redBox->color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);  // Colors.red
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->width = std::optional<float>(200.0f);
    redSized->height = std::optional<float>(150.0f);
    redSized->setChild(redBox);

    // Green expanded
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);  // Colors.green
    auto greenExpanded = std::make_shared<cw::RenderSizedBox>();
    greenExpanded->setChild(greenBox);

    // Blue box
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Colors.blue
    auto blueSized = std::make_shared<cw::RenderSizedBox>();
    blueSized->width = std::optional<float>(200.0f);
    blueSized->height = std::optional<float>(150.0f);
    blueSized->setChild(blueBox);

    root->insertChild(redSized, 0, 0);
    root->insertChild(greenExpanded, 1, 1);
    root->insertChild(blueSized, 2, 0);
    root->cross_axis_alignment = cw::CrossAxisAlignment::center;

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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, AlignCenter)
{
    // Center(child: Container(width: 300, height: 300, color: Colors.orange))

    auto orangeBox = std::make_shared<cw::RenderColoredBox>();
    orangeBox->color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f);  // Colors.orange

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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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
    purpleBox->color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f);  // Colors.purple

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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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
    root->cross_axis_alignment = cw::CrossAxisAlignment::center;

    auto redBox = std::make_shared<cw::RenderColoredBox>();
    redBox->color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);  // Colors.red
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->height = std::optional<float>(100.0f);
    redSized->setChild(redBox);

    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);  // Colors.green
    auto greenExpanded = std::make_shared<cw::RenderSizedBox>();
    greenExpanded->setChild(greenBox);

    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Colors.blue
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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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
    greyBox->color = cw::Color::fromRGB(0.8784f, 0.8784f, 0.8784f);  // Colors.grey.shade300
    auto bgSized = std::make_shared<cw::RenderSizedBox>();
    bgSized->setChild(greyBox);

    // Red box at (100, 100)
    auto redBox = std::make_shared<cw::RenderColoredBox>();
    redBox->color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);  // Colors.red
    auto redSized = std::make_shared<cw::RenderSizedBox>();
    redSized->width = std::optional<float>(250.0f);
    redSized->height = std::optional<float>(200.0f);
    redSized->setChild(redBox);

    // Blue box at bottom-right
    auto blueBox = std::make_shared<cw::RenderColoredBox>();
    blueBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Colors.blue
    auto blueSized = std::make_shared<cw::RenderSizedBox>();
    blueSized->width = std::optional<float>(250.0f);
    blueSized->height = std::optional<float>(200.0f);
    blueSized->setChild(blueBox);

    // Green box with opacity at (450, 300)
    auto greenBox = std::make_shared<cw::RenderColoredBox>();
    greenBox->color = cw::Color::fromRGBA(0.2980f, 0.6863f, 0.3137f, 0.6980f);  // Colors.green.withAlpha(178)
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
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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

    // Blue rectangle — Colors.blue
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), bluePaint);

    // Red circle — Colors.red
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);
    canvas.drawCircle(cw::Offset(200 * scaleX, 60 * scaleY), 40 * scaleX, redPaint);

    // Green oval — Colors.green
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    canvas.drawOval(cw::Rect::fromLTWH(280 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), greenPaint);

    // Purple stroked rectangle — Colors.purple
    cw::Paint purpleStroke;
    purpleStroke.color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f);
    purpleStroke.style = cw::PaintStyle::stroke;
    purpleStroke.stroke_width = 4 * scaleX;
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 120 * scaleY, 100 * scaleX, 80 * scaleY), purpleStroke);

    // Render to PNG
    std::string outputPath = getCppOutputPath("canvas_basic_shapes.png");
    bool success = false;
    cwt::GpuVisualRenderer gpuR(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    if (gpuR.isValid()) {
        gpuR.setClearColor(cw::Color::white());
        if (gpuR.renderDrawList(canvas.commands()))
            success = gpuR.saveToPng(outputPath);
    }
    if (!success) {
        cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
        renderer.clear(cw::Color::white());
        renderer.renderDrawList(canvas.commands());
        success = renderer.saveToPng(outputPath);
    }
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_basic_shapes.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_basic_shapes.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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

    // Single rounded rect — Colors.blue
    cw::RRect rrect1(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY), 20.0f * scaleX);
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawRRect(rrect1, bluePaint);

    // Different corner radii (simplified to use uniform radius) — Colors.green
    cw::RRect rrect2(cw::Rect::fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY), 30.0f * scaleX);
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    canvas.drawRRect(rrect2, greenPaint);

    // Double rounded rect — Colors.purple
    cw::RRect outerRRect(cw::Rect::fromLTWH(20 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 25.0f * scaleX);
    cw::RRect innerRRect(cw::Rect::fromLTWH(35 * scaleX, 165 * scaleY, 120 * scaleX, 70 * scaleY), 15.0f * scaleX);
    cw::Paint purplePaint;
    purplePaint.color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f);
    canvas.drawDRRect(outerRRect, innerRRect, purplePaint);

    // Stroked rounded rect — Colors.red
    cw::RRect rrect3(cw::Rect::fromLTWH(200 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 15.0f * scaleX);
    cw::Paint redStroke;
    redStroke.color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);
    redStroke.style = cw::PaintStyle::stroke;
    redStroke.stroke_width = 5 * scaleX;
    canvas.drawRRect(rrect3, redStroke);

    std::string outputPath = getCppOutputPath("canvas_rounded_rects.png");
    bool success = false;
    cwt::GpuVisualRenderer gpuR2(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    if (gpuR2.isValid()) {
        gpuR2.setClearColor(cw::Color::white());
        if (gpuR2.renderDrawList(canvas.commands()))
            success = gpuR2.saveToPng(outputPath);
    }
    if (!success) {
        cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
        renderer.clear(cw::Color::white());
        renderer.renderDrawList(canvas.commands());
        success = renderer.saveToPng(outputPath);
    }
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_rounded_rects.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_rounded_rects.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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

    // Thick orange line (8px stroke) — Colors.orange
    cw::Paint thickPaint;
    thickPaint.color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f);
    thickPaint.stroke_width = 8 * scaleX;
    canvas.drawLine(cw::Offset(50 * scaleX, 200 * scaleY), cw::Offset(350 * scaleX, 250 * scaleY), thickPaint);

    // Green lines with varying widths — Colors.green
    for (int i = 0; i < 4; i++) {
        float width = (i + 1) * 3.0f * scaleX;
        cw::Paint paint;
        paint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
        paint.stroke_width = width;
        float y = 300.0f * scaleY + i * 25 * scaleY;
        canvas.drawLine(cw::Offset(20 * scaleX, y), cw::Offset(150 * scaleX, y), paint);
    }

    std::string outputPath = getCppOutputPath("canvas_lines.png");
    bool success = false;
    cwt::GpuVisualRenderer gpuR3(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    if (gpuR3.isValid()) {
        gpuR3.setClearColor(cw::Color::white());
        if (gpuR3.renderDrawList(canvas.commands()))
            success = gpuR3.saveToPng(outputPath);
    }
    if (!success) {
        cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
        renderer.clear(cw::Color::white());
        renderer.renderDrawList(canvas.commands());
        success = renderer.saveToPng(outputPath);
    }
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_lines.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_lines.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
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
        cw::Color::fromRGB(0.7333f, 0.8706f, 0.9843f),  // Colors.blue.shade100
        cw::Color::fromRGB(0.8824f, 0.7451f, 0.9059f),  // Colors.purple.shade100
    };

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            cw::Paint paint;
            paint.color = bgColors[(i + j) % 2];
            paint.color.a = 76.0f / 255.0f;  // withAlpha(76)
            float cx = 40.0f * scaleX + i * 80 * scaleX;
            float cy = 40.0f * scaleY + j * 80 * scaleY;
            canvas.drawCircle(cw::Offset(cx, cy), 50 * scaleX, paint);
        }
    }

    // Central transformed group (card)
    canvas.save();
    canvas.translate(200 * scaleX, 200 * scaleY);

    // Card background (rounded rect)
    cw::RRect cardRRect(cw::Rect::fromLTWH(-120 * scaleX, -80 * scaleY, 240 * scaleX, 160 * scaleY), 20.0f * scaleX);

    cw::Paint cardPaint;
    cardPaint.color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 229.0f / 255.0f);  // Colors.white.withAlpha(229)
    canvas.drawRRect(cardRRect, cardPaint);

    // Card border
    cw::Paint borderPaint;
    borderPaint.color = cw::Color::fromRGB(0.3922f, 0.7098f, 0.9647f);  // Colors.blue.shade300
    borderPaint.style = cw::PaintStyle::stroke;
    borderPaint.stroke_width = 3 * scaleX;
    canvas.drawRRect(cardRRect, borderPaint);

    // Inner content - clipped to card
    canvas.save();
    canvas.clipRRect(cardRRect);

    // Colored circles inside card
    cw::Color circleColors[3] = {
        cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f),  // Colors.red
        cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f),  // Colors.green
        cw::Color::fromRGB(1.0f,    0.5961f, 0.0f),     // Colors.orange
    };

    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = circleColors[i];
        paint.color.a = 153.0f / 255.0f;  // withAlpha(153)
        float x = -60.0f * scaleX + i * 60 * scaleX;
        canvas.drawCircle(cw::Offset(x, 0), 35 * scaleX, paint);
    }

    canvas.restore();  // End clip
    canvas.restore();  // End transform

    canvas.restore();  // End scene

    cwt::GpuVisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.setClearColor(cw::Color::white());
    if (!renderer.isValid()) { GTEST_SKIP() << "GPU renderer unavailable"; return; }
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_complex_scene.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_complex_scene.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_complex_scene.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Utility Test
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Canvas Text Tests
// ----------------------------------------------------------------------------

TEST(VisualFidelity, CanvasTextBasic)
{
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Title — large blue
    {
        cw::TextSpan span;
        span.text = "Hello, World!";
        span.style.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f); // Colors.blue
        span.style.font_size = 48.0f;
        span.style.font_family = "Roboto";
        canvas.drawText(span, cw::Offset(50.0f, 50.0f));
    }

    // Bold red
    {
        cw::TextSpan span;
        span.text = "Bold text sample";
        span.style.color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f); // Colors.red
        span.style.font_size = 36.0f;
        span.style.font_family = "Roboto";
        span.style.font_weight = cw::FontWeight::bold;
        canvas.drawText(span, cw::Offset(50.0f, 140.0f));
    }

    // Green normal
    {
        cw::TextSpan span;
        span.text = "Regular green text";
        span.style.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f); // Colors.green
        span.style.font_size = 30.0f;
        span.style.font_family = "Roboto";
        canvas.drawText(span, cw::Offset(50.0f, 230.0f));
    }

    // Small black caption
    {
        cw::TextSpan span;
        span.text = "Small caption text (18px)";
        span.style.color = cw::Color::black();
        span.style.font_size = 18.0f;
        span.style.font_family = "Roboto";
        canvas.drawText(span, cw::Offset(50.0f, 315.0f));
    }

    // Large purple heading
    {
        cw::TextSpan span;
        span.text = "Large Heading";
        span.style.color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f); // Colors.purple
        span.style.font_size = 64.0f;
        span.style.font_family = "Roboto";
        span.style.font_weight = cw::FontWeight::bold;
        canvas.drawText(span, cw::Offset(50.0f, 370.0f));
    }

    cwt::GpuVisualRenderer renderer(static_cast<int>(kFidelityWidth), static_cast<int>(kFidelityHeight));
    renderer.setClearColor(cw::Color::white());
    if (!renderer.isValid()) { GTEST_SKIP() << "GPU renderer unavailable"; return; }
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_text_basic.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_text_basic.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_text_basic.png"),
            outputPath, 5, true);
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 20.0)
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Canvas Transform Tests
// ----------------------------------------------------------------------------

TEST(VisualFidelity, CanvasTransforms)
{
    // Tests translate and scale transforms with circles, rrects, and lines.
    // Rectangles are also included to verify they respect the transform AABB.
    const float W = kFidelityWidth;
    const float H = kFidelityHeight;

    cw::Canvas canvas(W, H);

    // — Translate: shift groups of shapes by a fixed offset —

    // Reference circle (no transform)
    {
        cw::Paint p; p.color = cw::Color::fromRGB(0.8784f, 0.8784f, 0.8784f);
        canvas.drawCircle(cw::Offset(100.0f, 130.0f), 60.0f, p);
    }

    // Translate right by 250
    canvas.save();
    canvas.translate(250.0f, 0.0f);
    {
        cw::Paint p; p.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f); // blue
        canvas.drawCircle(cw::Offset(100.0f, 130.0f), 60.0f, p);
    }
    canvas.restore();

    // Translate right + down
    canvas.save();
    canvas.translate(500.0f, 80.0f);
    {
        cw::Paint p; p.color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f); // red
        canvas.drawCircle(cw::Offset(100.0f, 130.0f), 60.0f, p);
    }
    canvas.restore();

    // — Scale: enlarge / shrink around a pivot —

    // Scale 2× (pivot = center of shape, placed at 900, 130)
    canvas.save();
    canvas.translate(900.0f, 130.0f);
    canvas.scale(2.0f, 2.0f);
    {
        cw::RRect rr(cw::Rect::fromLTWH(-40.0f, -40.0f, 80.0f, 80.0f), 10.0f);
        cw::Paint p; p.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f); // green
        canvas.drawRRect(rr, p);
    }
    canvas.restore();

    // Scale 0.5× (shrink)
    canvas.save();
    canvas.translate(1150.0f, 130.0f);
    canvas.scale(0.5f, 0.5f);
    {
        cw::RRect rr(cw::Rect::fromLTWH(-80.0f, -80.0f, 160.0f, 160.0f), 20.0f);
        cw::Paint p; p.color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f); // orange
        canvas.drawRRect(rr, p);
    }
    canvas.restore();

    // — Combined translate + scale —

    canvas.save();
    canvas.translate(200.0f, 380.0f);
    canvas.scale(1.5f, 1.0f);
    {
        cw::Paint p; p.color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f); // purple
        canvas.drawCircle(cw::Offset(0.0f, 0.0f), 50.0f, p);
    }
    canvas.restore();

    canvas.save();
    canvas.translate(600.0f, 380.0f);
    canvas.scale(1.0f, 1.5f);
    {
        cw::RRect rr(cw::Rect::fromLTWH(-50.0f, -40.0f, 100.0f, 80.0f), 12.0f);
        cw::Paint p; p.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f); // blue
        canvas.drawRRect(rr, p);
    }
    canvas.restore();

    // — Translate with lines —
    canvas.save();
    canvas.translate(900.0f, 450.0f);
    {
        cw::Paint p; p.color = cw::Color::black(); p.stroke_width = 4.0f;
        canvas.drawLine(cw::Offset(-80.0f, 0.0f), cw::Offset(80.0f, 0.0f), p);
        canvas.drawLine(cw::Offset(0.0f, -60.0f), cw::Offset(0.0f, 60.0f), p);
    }
    canvas.restore();

    cwt::GpuVisualRenderer renderer(static_cast<int>(W), static_cast<int>(H));
    renderer.setClearColor(cw::Color::white());
    if (!renderer.isValid()) { GTEST_SKIP() << "GPU renderer unavailable"; return; }
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_transforms.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_transforms.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_transforms.png"),
            outputPath, 5, true);
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

TEST(VisualFidelity, CanvasRotate)
{
    // Tests rotation transforms. Only circles (rotation-invariant) and lines
    // (endpoints transformed) are used — rrects/rects are intentionally
    // excluded since rotation produces an AABB mismatch vs Flutter.
    const float W = kFidelityWidth;
    const float H = kFidelityHeight;
    const float cx = W * 0.5f;
    const float cy = H * 0.5f;

    cw::Canvas canvas(W, H);

    // Hub circle at center
    {
        cw::Paint p; p.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
        canvas.drawCircle(cw::Offset(cx, cy), 20.0f, p);
    }

    // 8 spokes + outer circles at even angles
    const cw::Color spokeColors[8] = {
        cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f),
        cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f),
        cw::Color::fromRGB(1.0f,    0.5961f, 0.0f),
        cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f),
        cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f),
        cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f),
        cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f),
        cw::Color::fromRGB(1.0f,    0.5961f, 0.0f),
    };

    for (int i = 0; i < 8; ++i)
    {
        const float angle = i * (3.14159265f / 4.0f); // 45° increments
        canvas.save();
        canvas.translate(cx, cy);
        canvas.rotate(angle);

        // Spoke line
        cw::Paint lp; lp.color = spokeColors[i]; lp.stroke_width = 3.0f;
        canvas.drawLine(cw::Offset(0.0f, 0.0f), cw::Offset(220.0f, 0.0f), lp);

        // Outer circle at end of spoke
        cw::Paint cp; cp.color = spokeColors[i];
        canvas.drawCircle(cw::Offset(240.0f, 0.0f), 28.0f, cp);

        canvas.restore();
    }

    // Inner ring of small circles (rotate the whole ring 22.5°)
    canvas.save();
    canvas.translate(cx, cy);
    canvas.rotate(3.14159265f / 8.0f); // 22.5°
    for (int i = 0; i < 8; ++i)
    {
        const float a = i * (3.14159265f / 4.0f);
        canvas.save();
        canvas.rotate(a);
        cw::Paint p; p.color = cw::Color::fromRGBA(0.1294f, 0.5882f, 0.9529f, 0.4f);
        canvas.drawCircle(cw::Offset(110.0f, 0.0f), 14.0f, p);
        canvas.restore();
    }
    canvas.restore();

    cwt::GpuVisualRenderer renderer(static_cast<int>(W), static_cast<int>(H));
    renderer.setClearColor(cw::Color::white());
    if (!renderer.isValid()) { GTEST_SKIP() << "GPU renderer unavailable"; return; }
    renderer.renderDrawList(canvas.commands());

    std::string outputPath = getCppOutputPath("canvas_rotate.png");
    bool success = renderer.saveToPng(outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("canvas_rotate.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("canvas_rotate.png"),
            outputPath, 5, true);
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Widget Text Tests
// ----------------------------------------------------------------------------

TEST(VisualFidelity, WidgetTextColumn)
{
    // A vertical column of RenderText nodes with varying styles.
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;
    root->cross_axis_alignment = cw::CrossAxisAlignment::start;

    auto makePadded = [](std::shared_ptr<cw::RenderBox> child, float v, float h)
    {
        auto pad = std::make_shared<cw::RenderPadding>();
        pad->padding = cw::EdgeInsets::symmetric(v, h);
        pad->setChild(child);
        return pad;
    };

    // Title — large blue bold
    {
        cw::TextSpan span;
        span.text = "Widget Text Test";
        span.style.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
        span.style.font_size = 48.0f;
        span.style.font_family = "Roboto";
        span.style.font_weight = cw::FontWeight::bold;
        auto t = std::make_shared<cw::RenderText>(); t->setTextSpan(span);
        root->insertChild(makePadded(t, 20.0f, 40.0f), 0, 0);
    }

    // Subtitle — medium black
    {
        cw::TextSpan span;
        span.text = "Subtitle in regular weight";
        span.style.color = cw::Color::black();
        span.style.font_size = 28.0f;
        span.style.font_family = "Roboto";
        auto t = std::make_shared<cw::RenderText>(); t->setTextSpan(span);
        root->insertChild(makePadded(t, 8.0f, 40.0f), 1, 0);
    }

    // Body — small grey
    {
        cw::TextSpan span;
        span.text = "Body text at 20px in grey color for readability";
        span.style.color = cw::Color::fromRGB(0.4667f, 0.4667f, 0.4667f);
        span.style.font_size = 20.0f;
        span.style.font_family = "Roboto";
        auto t = std::make_shared<cw::RenderText>(); t->setTextSpan(span);
        root->insertChild(makePadded(t, 8.0f, 40.0f), 2, 0);
    }

    // Bold red accent
    {
        cw::TextSpan span;
        span.text = "Bold red accent text";
        span.style.color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);
        span.style.font_size = 24.0f;
        span.style.font_family = "Roboto";
        span.style.font_weight = cw::FontWeight::bold;
        auto t = std::make_shared<cw::RenderText>(); t->setTextSpan(span);
        root->insertChild(makePadded(t, 8.0f, 40.0f), 3, 0);
    }

    // Small caption
    {
        cw::TextSpan span;
        span.text = "Small caption — 14px Roboto";
        span.style.color = cw::Color::fromRGB(0.6f, 0.6f, 0.6f);
        span.style.font_size = 14.0f;
        span.style.font_family = "Roboto";
        auto t = std::make_shared<cw::RenderText>(); t->setTextSpan(span);
        root->insertChild(makePadded(t, 8.0f, 40.0f), 4, 0);
    }

    std::string outputPath = getCppOutputPath("widget_text_column.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("widget_text_column.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("widget_text_column.png"),
            outputPath, 5, true);
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 20.0)
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

// ----------------------------------------------------------------------------
// Widget Transform Tests
// ----------------------------------------------------------------------------

TEST(VisualFidelity, WidgetTransform)
{
    // Tests RenderTransform (translate, scale) wrapping colored boxes.
    // Rotation is omitted here because axis-aligned drawRect under rotation
    // produces a larger AABB than Flutter's OBB rendering; use CanvasRotate
    // for rotation fidelity.
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;
    root->cross_axis_alignment = cw::CrossAxisAlignment::center;

    auto makeBox = [](cw::Color color, float w, float h) {
        auto box = std::make_shared<cw::RenderColoredBox>();
        box->color = color;
        auto sized = std::make_shared<cw::RenderSizedBox>();
        sized->width = w; sized->height = h;
        sized->setChild(box);
        return sized;
    };

    auto makePadded = [](std::shared_ptr<cw::RenderBox> child, float all) {
        auto pad = std::make_shared<cw::RenderPadding>();
        pad->padding = cw::EdgeInsets::all(all);
        pad->setChild(child);
        return pad;
    };

    // Row 1: plain box (reference, no transform)
    {
        auto box = makeBox(cw::Color::fromRGB(0.8784f, 0.8784f, 0.8784f), 300.0f, 80.0f);
        root->insertChild(makePadded(box, 20.0f), 0, 0);
    }

    // Row 2: translate right 60px (Alignment::topLeft — no pivot offset for translate)
    {
        auto t = std::make_shared<cw::RenderTransform>();
        t->transform  = cw::RenderTransform::translation(60.0f, 0.0f);
        t->alignment  = cw::Alignment::topLeft();
        t->setChild(makeBox(cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f), 300.0f, 80.0f));
        root->insertChild(makePadded(t, 20.0f), 1, 0);
    }

    // Row 3: scale 0.6× (Alignment::center — shrink from center)
    {
        auto t = std::make_shared<cw::RenderTransform>();
        t->transform  = cw::RenderTransform::scaling(0.6f);
        t->alignment  = cw::Alignment::center();
        t->setChild(makeBox(cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f), 300.0f, 80.0f));
        root->insertChild(makePadded(t, 20.0f), 2, 0);
    }

    // Row 4: scale(1.4, 0.7) non-uniform
    {
        auto t = std::make_shared<cw::RenderTransform>();
        t->transform  = cw::RenderTransform::scaling(1.4f, 0.7f);
        t->alignment  = cw::Alignment::center();
        t->setChild(makeBox(cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f), 300.0f, 80.0f));
        root->insertChild(makePadded(t, 20.0f), 3, 0);
    }

    // Row 5: translate left 40px + scale 1.2×
    {
        auto inner = std::make_shared<cw::RenderTransform>();
        inner->transform = cw::RenderTransform::scaling(1.2f);
        inner->alignment = cw::Alignment::center();
        inner->setChild(makeBox(cw::Color::fromRGB(1.0f, 0.5961f, 0.0f), 300.0f, 80.0f));

        auto outer = std::make_shared<cw::RenderTransform>();
        outer->transform = cw::RenderTransform::translation(-40.0f, 0.0f);
        outer->alignment = cw::Alignment::topLeft();
        outer->setChild(inner);
        root->insertChild(makePadded(outer, 20.0f), 4, 0);
    }

    std::string outputPath = getCppOutputPath("widget_transform.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath);
    EXPECT_TRUE(success);

    if (flutterGoldenExists("widget_transform.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("widget_transform.png"),
            outputPath, 5, true);
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

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
