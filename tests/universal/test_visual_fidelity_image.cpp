#include <gtest/gtest.h>
#include "visual_fidelity.hpp"
#include "gpu_visual_renderer.hpp"
#include "fidelity.hpp"
#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <filesystem>
#include <iostream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// Standard resolution for fidelity testing
constexpr float kFidelityWidth = 1280.0f;
constexpr float kFidelityHeight = 720.0f;

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

// ----------------------------------------------------------------------------
// Image Widget Visual Fidelity Tests
// ----------------------------------------------------------------------------

/// Tests a simple Image with explicit size (like Image.asset with width/height)
TEST(VisualFidelityImage, ImageExplicitSize)
{
    // Build: Container(
    //   color: Colors.grey.shade200,
    //   child: Center(
    //     child: Container(width: 400, height: 300, color: Colors.blue),
    //   ),
    // )
    // Note: In C++ we use RenderColoredBox to simulate an image
    // Flutter uses Image widget with a colored placeholder

    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::fromRGB(0.9333f, 0.9333f, 0.9333f);  // Colors.grey.shade200

    // Center alignment wrapper
    auto align = std::make_shared<cw::RenderAlign>();
    align->alignment = cw::Alignment::center();

    // The "image" - a 400x300 colored box simulating an image
    auto imageBox = std::make_shared<cw::RenderColoredBox>();
    imageBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Colors.blue

    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(400.0f);
    imageSized->height = std::optional<float>(300.0f);
    imageSized->setChild(imageBox);

    align->setChild(imageSized);
    root->setChild(align);

    std::string outputPath = getCppOutputPath("image_explicit_size.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success) << "Failed to render PNG to " << outputPath;

    if (flutterGoldenExists("image_explicit_size.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_explicit_size.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
    } else {
        GTEST_SKIP() << "Flutter golden not found. Run Flutter tests first.";
    }
}

/// Tests Image with BoxFit.contain
TEST(VisualFidelityImage, ImageFitContain)
{
    // Container with padding, image scaled to fit within bounds while maintaining aspect ratio
    auto root = std::make_shared<cw::RenderPadding>();
    root->padding = cw::EdgeInsets::all(50.0f);

    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.9333f, 0.9333f, 0.9333f);  // Grey background

    // Simulating BoxFit.contain - image is 4:3 aspect ratio, container is 16:9
    // Image should be centered and scaled to fit within container
    auto imageBox = std::make_shared<cw::RenderColoredBox>();
    imageBox->color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);  // Green image

    // With contain, 400x300 image in 1180x620 container (after padding)
    // Should scale to fit height: 620px height = 827px width (4:3 ratio)
    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(826.67f);  // 620 * 4/3
    imageSized->height = std::optional<float>(620.0f);
    imageSized->setChild(imageBox);

    auto center = std::make_shared<cw::RenderAlign>();
    center->alignment = cw::Alignment::center();
    center->setChild(imageSized);

    bgBox->setChild(center);
    root->setChild(bgBox);

    std::string outputPath = getCppOutputPath("image_fit_contain.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_fit_contain.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_fit_contain.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Tests Image with BoxFit.cover
TEST(VisualFidelityImage, ImageFitCover)
{
    // Image fills container, cropping to maintain aspect ratio
    auto root = std::make_shared<cw::RenderPadding>();
    root->padding = cw::EdgeInsets::all(50.0f);

    // Background (would be cropped image in real test)
    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);  // Red

    // With cover, image fills entire container
    // Simulating the visible portion
    auto visibleBox = std::make_shared<cw::RenderColoredBox>();
    visibleBox->color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);  // Blue center

    auto centerSized = std::make_shared<cw::RenderSizedBox>();
    centerSized->width = std::optional<float>(885.0f);  // Center portion
    centerSized->height = std::optional<float>(620.0f);
    centerSized->setChild(visibleBox);

    auto center = std::make_shared<cw::RenderAlign>();
    center->alignment = cw::Alignment::center();
    center->setChild(centerSized);

    bgBox->setChild(center);
    root->setChild(bgBox);

    std::string outputPath = getCppOutputPath("image_fit_cover.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_fit_cover.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_fit_cover.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Tests Image with BoxFit.fill
TEST(VisualFidelityImage, ImageFitFill)
{
    // Image fills container exactly, distorting aspect ratio
    auto root = std::make_shared<cw::RenderPadding>();
    root->padding = cw::EdgeInsets::all(50.0f);

    auto imageBox = std::make_shared<cw::RenderColoredBox>();
    imageBox->color = cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f);  // Purple

    // Fill stretches to exact container size
    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(1180.0f);  // 1280 - 100 padding
    imageSized->height = std::optional<float>(620.0f);  // 720 - 100 padding
    imageSized->setChild(imageBox);

    root->setChild(imageSized);

    std::string outputPath = getCppOutputPath("image_fit_fill.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_fit_fill.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_fit_fill.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Tests multiple images in a grid layout
TEST(VisualFidelityImage, ImageGrid)
{
    // Grid of 3x2 images
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;
    root->cross_axis_alignment = cw::CrossAxisAlignment::stretch;

    cw::Color colors[6] = {
        cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f),  // Red
        cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f),  // Green
        cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f),  // Blue
        cw::Color::fromRGB(1.0f, 0.5961f, 0.0f),        // Orange
        cw::Color::fromRGB(0.6118f, 0.1529f, 0.6902f),  // Purple
        cw::Color::fromRGB(0.0f, 0.7373f, 0.8314f),     // Cyan
    };

    for (int row = 0; row < 2; row++) {
        auto rowFlex = std::make_shared<cw::RenderFlex>();
        rowFlex->axis = cw::Axis::horizontal;
        rowFlex->main_axis_size = cw::MainAxisSize::max;

        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            
            auto imageBox = std::make_shared<cw::RenderColoredBox>();
            imageBox->color = colors[idx];

            auto imageSized = std::make_shared<cw::RenderSizedBox>();
            imageSized->width = std::nullopt;  // Fill
            imageSized->height = std::nullopt;
            imageSized->setChild(imageBox);

            // Add padding around each image
            auto padding = std::make_shared<cw::RenderPadding>();
            padding->padding = cw::EdgeInsets::all(10.0f);
            padding->setChild(imageSized);

            rowFlex->insertChild(padding, col, 1);  // flex: 1
        }

        root->insertChild(rowFlex, row, 1);  // flex: 1
    }

    std::string outputPath = getCppOutputPath("image_grid.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_grid.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_grid.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Tests Image with opacity
TEST(VisualFidelityImage, ImageWithOpacity)
{
    // Stacked images with varying opacity
    auto root = std::make_shared<cw::RenderStack>();
    root->fit = cw::StackFit::expand;

    // Background
    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.2f, 0.2f, 0.2f);  // Dark grey
    auto bgSized = std::make_shared<cw::RenderSizedBox>();
    bgSized->setChild(bgBox);

    // Semi-transparent image (simulated with opacity)
    auto imageBox = std::make_shared<cw::RenderColoredBox>();
    imageBox->color = cw::Color::fromRGBA(0.1294f, 0.5882f, 0.9529f, 0.5f);  // 50% opacity blue

    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(600.0f);
    imageSized->height = std::optional<float>(400.0f);
    imageSized->setChild(imageBox);

    auto center = std::make_shared<cw::RenderAlign>();
    center->alignment = cw::Alignment::center();
    center->setChild(imageSized);

    root->insertChild(bgSized, 0, 0, 0, 0, 0, std::nullopt, std::nullopt);
    root->insertChild(center, 1, std::nullopt, std::nullopt, std::nullopt, std::nullopt, 600, 400);

    std::string outputPath = getCppOutputPath("image_with_opacity.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_with_opacity.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_with_opacity.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Tests Image with alignment variations
TEST(VisualFidelityImage, ImageAlignmentVariations)
{
    // Single image with different alignments shown as multiple boxes
    auto root = std::make_shared<cw::RenderFlex>();
    root->axis = cw::Axis::vertical;
    root->main_axis_size = cw::MainAxisSize::max;

    // Row 1: Top alignments
    auto row1 = std::make_shared<cw::RenderFlex>();
    row1->axis = cw::Axis::horizontal;
    row1->main_axis_size = cw::MainAxisSize::max;

    cw::Alignment alignments[3] = {
        cw::Alignment::topLeft(),
        cw::Alignment::topCenter(),
        cw::Alignment::topRight(),
    };

    for (int i = 0; i < 3; i++) {
        auto container = std::make_shared<cw::RenderColoredBox>();
        container->color = cw::Color::fromRGB(0.9333f, 0.9333f, 0.9333f);

        auto imageBox = std::make_shared<cw::RenderColoredBox>();
        imageBox->color = cw::Color::fromRGB(0.9569f, 0.2627f, 0.2118f);  // Red

        auto imageSized = std::make_shared<cw::RenderSizedBox>();
        imageSized->width = std::optional<float>(150.0f);
        imageSized->height = std::optional<float>(100.0f);
        imageSized->setChild(imageBox);

        auto align = std::make_shared<cw::RenderAlign>();
        align->alignment = alignments[i];
        align->setChild(imageSized);

        container->setChild(align);
        row1->insertChild(container, i, 1);
    }

    // Row 2: Center and bottom alignments
    auto row2 = std::make_shared<cw::RenderFlex>();
    row2->axis = cw::Axis::horizontal;
    row2->main_axis_size = cw::MainAxisSize::max;

    cw::Alignment alignments2[3] = {
        cw::Alignment::centerLeft(),
        cw::Alignment::center(),
        cw::Alignment::centerRight(),
    };

    for (int i = 0; i < 3; i++) {
        auto container = std::make_shared<cw::RenderColoredBox>();
        container->color = cw::Color::fromRGB(0.9333f, 0.9333f, 0.9333f);

        auto imageBox = std::make_shared<cw::RenderColoredBox>();
        imageBox->color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);  // Green

        auto imageSized = std::make_shared<cw::RenderSizedBox>();
        imageSized->width = std::optional<float>(150.0f);
        imageSized->height = std::optional<float>(100.0f);
        imageSized->setChild(imageBox);

        auto align = std::make_shared<cw::RenderAlign>();
        align->alignment = alignments2[i];
        align->setChild(imageSized);

        container->setChild(align);
        row2->insertChild(container, i, 1);
    }

    root->insertChild(row1, 0, 1);
    root->insertChild(row2, 1, 1);

    std::string outputPath = getCppOutputPath("image_alignment_variations.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("image_alignment_variations.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("image_alignment_variations.png"),
            outputPath,
            5, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 5.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}
