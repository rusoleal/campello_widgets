#include <gtest/gtest.h>
#include "visual_fidelity.hpp"
#include "gpu_visual_renderer.hpp"
#include "fidelity.hpp"
#include "visual_fidelity_helpers.hpp"
#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>

// STB image for loading JPEG/PNG
#include "../third_party/stb_image.h"

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;
namespace cg = systems::leal::campello_gpu;

using cwt::kFidelityWidth;
using cwt::kFidelityHeight;
using cwt::flutterGoldenExists;
using cwt::getFlutterGoldenPath;
using cwt::getCppOutputPath;

// ----------------------------------------------------------------------------
// Helper: Load image from file into a GPU texture
// ----------------------------------------------------------------------------

static std::shared_ptr<cg::Texture> loadTextureFromFile(
    const std::string& path,
    std::shared_ptr<cg::Device> device,
    int& outWidth,
    int& outHeight)
{
    if (!std::filesystem::exists(path)) {
        std::cerr << "Image file not found: " << path << std::endl;
        return nullptr;
    }

    // Load image with stb_image
    int channels;
    unsigned char* data = stbi_load(path.c_str(), &outWidth, &outHeight, &channels, 4);  // Force RGBA
    if (!data) {
        std::cerr << "Failed to load image: " << path << std::endl;
        return nullptr;
    }

    // Create texture
    auto texture = device->createTexture(
        cg::TextureType::tt2d,
        cg::PixelFormat::rgba8unorm,
        outWidth, outHeight, 1, 1, 1,
        static_cast<cg::TextureUsage>(
            static_cast<int>(cg::TextureUsage::textureBinding) |
            static_cast<int>(cg::TextureUsage::copyDst))
    );

    if (!texture) {
        std::cerr << "Failed to create texture for: " << path << std::endl;
        stbi_image_free(data);
        return nullptr;
    }

    // Upload pixel data
    size_t dataSize = outWidth * outHeight * 4;
    bool uploaded = texture->upload(0, dataSize, data);
    stbi_image_free(data);

    if (!uploaded) {
        std::cerr << "Failed to upload texture data for: " << path << std::endl;
        return nullptr;
    }

    return texture;
}

// Helper: Get test images directory
static std::string getTestImagesDirectory()
{
    std::filesystem::path paths[] = {
        "tests/visual_fidelity/test_images",
        "../tests/visual_fidelity/test_images",
        "../../tests/visual_fidelity/test_images",
        "../../../tests/visual_fidelity/test_images",
    };

    for (const auto& p : paths) {
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }
    return "tests/visual_fidelity/test_images";
}

static std::string getTestImagePath(const std::string& filename)
{
    return (std::filesystem::path(getTestImagesDirectory()) / filename).string();
}

// ----------------------------------------------------------------------------
// Real Image Visual Fidelity Tests
// ----------------------------------------------------------------------------

/// Test 1: Display a single real image at explicit size
TEST(VisualFidelityImageReal, SingleImageExplicitSize)
{
    auto device = cg::Device::createDefaultDevice(nullptr);
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    // Load the test image
    int imgWidth, imgHeight;
    auto texture = loadTextureFromFile(getTestImagePath("sample1.jpg"), device, imgWidth, imgHeight);
    if (!texture) {
        GTEST_SKIP() << "Failed to load test image";
        return;
    }

    // White background (matches Flutter's Scaffold backgroundColor: Colors.white)
    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    // Create RenderImage with the texture
    auto image = std::make_shared<cw::RenderImage>();
    image->setTexture(texture);
    image->setExplicitSize(cw::Size{400.0f, 300.0f});
    image->setFit(cw::BoxFit::fill);
    image->setAlignment(cw::Alignment::center());

    // Center the image (matches Flutter's Center widget)
    auto align = std::make_shared<cw::RenderAlign>();
    align->alignment = cw::Alignment::center();
    align->setChild(image);
    root->setChild(align);

    std::string outputPath = getCppOutputPath("real_image_explicit_size.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success) << "Failed to render PNG to " << outputPath;

    if (flutterGoldenExists("real_image_explicit_size.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_explicit_size.png"),
            outputPath,
            10, true  // Higher tolerance for JPEG compression differences
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 10.0)
            << "Visual difference too large: " << result.pixelDifference << "% of pixels differ";
    } else {
        GTEST_SKIP() << "Flutter golden not found. Run Flutter tests first.";
    }
}

/// Test 2: Image with BoxFit.contain (preserves aspect ratio)
TEST(VisualFidelityImageReal, ImageFitContain)
{
    auto device = cg::Device::createDefaultDevice(nullptr);
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    int imgWidth, imgHeight;
    auto texture = loadTextureFromFile(getTestImagePath("sample1.jpg"), device, imgWidth, imgHeight);
    if (!texture) {
        GTEST_SKIP() << "Failed to load test image";
        return;
    }

    // White background root (matches Flutter's Scaffold)
    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    // Grey background container with padding (matches Flutter's Padding + Container)
    auto container = std::make_shared<cw::RenderPadding>();
    container->padding = cw::EdgeInsets::all(50.0f);

    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);  // Colors.grey.shade300

    // Image with contain fit - should preserve aspect ratio
    auto image = std::make_shared<cw::RenderImage>();
    image->setTexture(texture);
    image->setExplicitSize(cw::Size{0.0f, 0.0f});  // Use constraints
    image->setFit(cw::BoxFit::contain);
    image->setAlignment(cw::Alignment::center());

    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(1180.0f);  // 1280 - 100 padding
    imageSized->height = std::optional<float>(620.0f);  // 720 - 100 padding
    imageSized->setChild(image);

    bgBox->setChild(imageSized);
    container->setChild(bgBox);
    root->setChild(container);

    std::string outputPath = getCppOutputPath("real_image_fit_contain.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_fit_contain.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_fit_contain.png"),
            outputPath,
            10, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 10.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Test 3: Image with BoxFit.cover (fills and crops)
TEST(VisualFidelityImageReal, ImageFitCover)
{
    auto device = cg::Device::createDefaultDevice(nullptr);
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    int imgWidth, imgHeight;
    auto texture = loadTextureFromFile(getTestImagePath("sample1.jpg"), device, imgWidth, imgHeight);
    if (!texture) {
        GTEST_SKIP() << "Failed to load test image";
        return;
    }

    // White background root (matches Flutter's Scaffold)
    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    auto container = std::make_shared<cw::RenderPadding>();
    container->padding = cw::EdgeInsets::all(50.0f);

    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);  // Colors.grey.shade300

    auto image = std::make_shared<cw::RenderImage>();
    image->setTexture(texture);
    image->setExplicitSize(cw::Size{0.0f, 0.0f});
    image->setFit(cw::BoxFit::cover);
    image->setAlignment(cw::Alignment::center());

    auto imageSized = std::make_shared<cw::RenderSizedBox>();
    imageSized->width = std::optional<float>(1180.0f);
    imageSized->height = std::optional<float>(620.0f);
    imageSized->setChild(image);

    bgBox->setChild(imageSized);
    container->setChild(bgBox);
    root->setChild(container);

    std::string outputPath = getCppOutputPath("real_image_fit_cover.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_fit_cover.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_fit_cover.png"),
            outputPath,
            10, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 10.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Test 4: Multiple different images in a grid
TEST(VisualFidelityImageReal, ImageGallery)
{
    auto device = cg::Device::createDefaultDevice(nullptr);
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    // Load multiple images
    const char* imageFiles[] = {"sample1.jpg", "sample2.jpg", "sample3.jpg"};
    std::shared_ptr<cg::Texture> textures[3];
    int widths[3], heights[3];

    for (int i = 0; i < 3; i++) {
        textures[i] = loadTextureFromFile(getTestImagePath(imageFiles[i]), device, widths[i], heights[i]);
        if (!textures[i]) {
            GTEST_SKIP() << "Failed to load test image: " << imageFiles[i];
            return;
        }
    }

    // White background root (matches Flutter's Scaffold)
    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    // Create a grid: 3 images side by side
    auto flex = std::make_shared<cw::RenderFlex>();
    flex->axis = cw::Axis::horizontal;
    flex->main_axis_size = cw::MainAxisSize::max;
    flex->cross_axis_alignment = cw::CrossAxisAlignment::stretch;

    for (int i = 0; i < 3; i++) {
        auto padding = std::make_shared<cw::RenderPadding>();
        padding->padding = cw::EdgeInsets::all(20.0f);

        auto image = std::make_shared<cw::RenderImage>();
        image->setTexture(textures[i]);
        image->setFit(cw::BoxFit::cover);
        image->setAlignment(cw::Alignment::center());

        auto imageSized = std::make_shared<cw::RenderSizedBox>();
        // Each image gets 1/3 of width minus padding
        imageSized->setChild(image);

        padding->setChild(imageSized);
        flex->insertChild(padding, i, 1);  // flex: 1
    }

    root->setChild(flex);

    std::string outputPath = getCppOutputPath("real_image_gallery.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_gallery.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_gallery.png"),
            outputPath,
            10, true
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 10.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}

/// Test 5: Image with opacity
TEST(VisualFidelityImageReal, ImageWithOpacity)
{
    auto device = cg::Device::createDefaultDevice(nullptr);
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    int imgWidth, imgHeight;
    auto texture = loadTextureFromFile(getTestImagePath("sample3.jpg"), device, imgWidth, imgHeight);
    if (!texture) {
        GTEST_SKIP() << "Failed to load test image";
        return;
    }

    // White background root (matches Flutter's Scaffold)
    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    // Stack with background and semi-transparent image
    auto stack = std::make_shared<cw::RenderStack>();
    stack->fit = cw::StackFit::expand;

    // Background pattern - match Flutter's blue/grey color
    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromRGB(0.2f, 0.3f, 0.4f);  // Colors.blueGrey

    auto bgSized = std::make_shared<cw::RenderSizedBox>();
    bgSized->setChild(bgBox);

    // Semi-transparent image
    auto image = std::make_shared<cw::RenderImage>();
    image->setTexture(texture);
    image->setExplicitSize(cw::Size{600.0f, 450.0f});
    image->setFit(cw::BoxFit::fill);
    image->setOpacity(0.6f);
    image->setAlignment(cw::Alignment::center());

    auto center = std::make_shared<cw::RenderAlign>();
    center->alignment = cw::Alignment::center();
    center->setChild(image);

    stack->insertChild(bgSized, 0, 0, 0, 0, 0, std::nullopt, std::nullopt);
    stack->insertChild(center, 1, std::nullopt, std::nullopt, std::nullopt, std::nullopt, 600, 450);

    root->setChild(stack);

    std::string outputPath = getCppOutputPath("real_image_with_opacity.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_with_opacity.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_with_opacity.png"),
            outputPath,
            15, true  // Higher tolerance for opacity differences
        );
        if (!result.diffImagePath.empty())
            std::cout << "Diff image: " << result.diffImagePath << std::endl;
        EXPECT_LT(result.pixelDifference, 15.0)
            << "Visual difference: " << result.pixelDifference << "%";
    } else {
        GTEST_SKIP() << "Flutter golden not found";
    }
}
