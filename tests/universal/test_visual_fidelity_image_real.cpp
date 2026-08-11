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
#include <campello_widgets/ui/render_clip_rrect.hpp>
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
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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
    // Matches Flutter's Row default (CrossAxisAlignment.center) — the Dart
    // golden's Row doesn't set crossAxisAlignment explicitly.
    flex->cross_axis_alignment = cw::CrossAxisAlignment::center;

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

    // Matches the Dart golden's explicit SizedBox(width, height) wrapper —
    // without it, RenderColoredBox loosens the constraints it passes down
    // (matching Flutter's Scaffold background), so the Row would only be
    // as tall as its tallest child instead of filling the full canvas
    // height, leaving no room for CrossAxisAlignment::center to center in.
    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width  = std::optional<float>(kFidelityWidth);
    sized->height = std::optional<float>(kFidelityHeight);
    sized->setChild(flex);

    root->setChild(sized);

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
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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

    // Background pattern - matches Flutter's const Color(0xFF334455)
    auto bgBox = std::make_shared<cw::RenderColoredBox>();
    bgBox->color = cw::Color::fromARGB(0xFF334455);

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
    stack->insertChild(center, 1, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

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

// ----------------------------------------------------------------------------
// BoxFit coverage — every other real-image test above only exercises
// contain/cover, and the "fake" (RenderColoredBox-simulated) tests in
// test_visual_fidelity_image.cpp never touch RenderImage's actual BoxFit
// switch at all. fill/fitWidth/fitHeight/none/scaleDown have never been
// verified against Flutter before.
// ----------------------------------------------------------------------------

TEST(VisualFidelityImageReal, BoxFitModes)
{
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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

    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width  = std::optional<float>(kFidelityWidth);
    sized->height = std::optional<float>(kFidelityHeight);

    auto stack = std::make_shared<cw::RenderStack>();

    // sample1.jpg is 400x300 (4:3) — a 260x180 box is small enough (and
    // not an exact aspect-ratio match) to force real scaling/cropping/
    // clipping decisions for every fit mode, unlike a box that happens to
    // share the image's exact aspect ratio.
    const struct { cw::BoxFit fit; float left; float top; } cells[] = {
        {cw::BoxFit::fill,      40.0f,  40.0f},
        {cw::BoxFit::contain,   340.0f, 40.0f},
        {cw::BoxFit::cover,     640.0f, 40.0f},
        {cw::BoxFit::fitWidth,  940.0f, 40.0f},
        {cw::BoxFit::fitHeight, 40.0f,  280.0f},
        {cw::BoxFit::none,      340.0f, 280.0f},
        {cw::BoxFit::scaleDown, 640.0f, 280.0f},
    };

    int index = 0;
    for (const auto& c : cells) {
        auto image = std::make_shared<cw::RenderImage>();
        image->setTexture(texture);
        image->setFit(c.fit);
        image->setAlignment(cw::Alignment::center());

        // Re-tightens the constraints RenderColoredBox loosens for its
        // child (matches Flutter's Container(color:...), which does not
        // loosen) — without this, RenderImage would fall back to its own
        // aspect-preserving auto-size instead of filling the 260x180 cell,
        // per RenderImage::performLayout()'s "no explicit size" branch.
        auto cellSized = std::make_shared<cw::RenderSizedBox>();
        cellSized->width  = std::optional<float>(260.0f);
        cellSized->height = std::optional<float>(180.0f);
        cellSized->setChild(image);

        auto bg = std::make_shared<cw::RenderColoredBox>();
        bg->color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);  // Colors.grey.shade300
        bg->setChild(cellSized);

        stack->insertChild(bg, index, c.left, c.top, std::nullopt, std::nullopt, 260.0f, 180.0f);
        ++index;
    }

    sized->setChild(stack);
    root->setChild(sized);

    std::string outputPath = getCppOutputPath("real_image_boxfit_modes.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_boxfit_modes.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_boxfit_modes.png"),
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

TEST(VisualFidelityImageReal, BoxFitScaleDownDoesNotUpscale)
{
    auto device = cwt::GpuVisualRenderer::sharedDevice();
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

    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width  = std::optional<float>(kFidelityWidth);
    sized->height = std::optional<float>(kFidelityHeight);

    auto stack = std::make_shared<cw::RenderStack>();

    // sample1.jpg is 400x300 — a 550x450 box is larger than the source in
    // both axes, so BoxFit.contain upscales it to fill the box, while
    // BoxFit.scaleDown must not — the only scenario where the two modes
    // actually differ (a box smaller than the source makes them identical).
    const struct { cw::BoxFit fit; float left; } cells[] = {
        {cw::BoxFit::contain,   40.0f},
        {cw::BoxFit::scaleDown, 650.0f},
    };

    int index = 0;
    for (const auto& c : cells) {
        auto image = std::make_shared<cw::RenderImage>();
        image->setTexture(texture);
        image->setFit(c.fit);
        image->setAlignment(cw::Alignment::center());

        auto cellSized = std::make_shared<cw::RenderSizedBox>();
        cellSized->width  = std::optional<float>(550.0f);
        cellSized->height = std::optional<float>(450.0f);
        cellSized->setChild(image);

        auto bg = std::make_shared<cw::RenderColoredBox>();
        bg->color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);
        bg->setChild(cellSized);

        stack->insertChild(bg, index, c.left, 100.0f, std::nullopt, std::nullopt, 550.0f, 450.0f);
        ++index;
    }

    sized->setChild(stack);
    root->setChild(sized);

    std::string outputPath = getCppOutputPath("real_image_boxfit_scaledown_cap.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_boxfit_scaledown_cap.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_boxfit_scaledown_cap.png"),
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

// Mirrors examples/gallery/gallery_app.cpp's fitSample() exactly (same
// 120x120 box, same background color, same 8px ClipRRect, same
// mountains.jpg source) — the actual widget tree shape the gallery's
// "Images" tab BoxFit showcase renders, as opposed to the more generic
// bare-RenderImage setup the other BoxFit tests above use.
TEST(VisualFidelityImageReal, BoxFitGalleryReplica)
{
    auto device = cwt::GpuVisualRenderer::sharedDevice();
    if (!device) {
        GTEST_SKIP() << "GPU device not available";
        return;
    }

    int imgWidth, imgHeight;
    auto texture = loadTextureFromFile(getTestImagePath("mountains.jpg"), device, imgWidth, imgHeight);
    if (!texture) {
        GTEST_SKIP() << "Failed to load test image";
        return;
    }

    auto root = std::make_shared<cw::RenderColoredBox>();
    root->color = cw::Color::white();

    auto sized = std::make_shared<cw::RenderSizedBox>();
    sized->width  = std::optional<float>(kFidelityWidth);
    sized->height = std::optional<float>(kFidelityHeight);

    auto stack = std::make_shared<cw::RenderStack>();

    const float kBoxSize = 120.0f;
    const struct { cw::BoxFit fit; float left; } cells[] = {
        {cw::BoxFit::fill,      40.0f},
        {cw::BoxFit::contain,   220.0f},
        {cw::BoxFit::cover,     400.0f},
        {cw::BoxFit::fitWidth,  580.0f},
        {cw::BoxFit::fitHeight, 760.0f},
        {cw::BoxFit::none,      940.0f},
        {cw::BoxFit::scaleDown, 1120.0f},
    };

    int index = 0;
    for (const auto& c : cells) {
        auto image = std::make_shared<cw::RenderImage>();
        image->setTexture(texture);
        image->setFit(c.fit);
        image->setAlignment(cw::Alignment::center());

        // Re-tightens what RenderColoredBox loosens for its child — see
        // BoxFitModes' comment above.
        auto reTight = std::make_shared<cw::RenderSizedBox>();
        reTight->width  = std::optional<float>(kBoxSize);
        reTight->height = std::optional<float>(kBoxSize);
        reTight->setChild(image);

        auto bg = std::make_shared<cw::RenderColoredBox>();
        bg->color = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);
        bg->setChild(reTight);

        auto clip = std::make_shared<cw::RenderClipRRect>();
        clip->border_radius = 8.0f;
        clip->setChild(bg);

        stack->insertChild(clip, index, c.left, 40.0f, std::nullopt, std::nullopt, kBoxSize, kBoxSize);
        ++index;
    }

    sized->setChild(stack);
    root->setChild(sized);

    std::string outputPath = getCppOutputPath("real_image_boxfit_gallery_replica.png");
    bool success = cwt::captureToPng(
        *root,
        cw::BoxConstraints::tight(kFidelityWidth, kFidelityHeight),
        kFidelityWidth, kFidelityHeight,
        outputPath
    );

    EXPECT_TRUE(success);

    if (flutterGoldenExists("real_image_boxfit_gallery_replica.png")) {
        auto result = cwt::comparePngImages(
            getFlutterGoldenPath("real_image_boxfit_gallery_replica.png"),
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
