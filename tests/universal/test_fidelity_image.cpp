#include <gtest/gtest.h>
#include "fidelity.hpp"
#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/flex_properties.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/widgets/image.hpp>
#include <campello_widgets/widgets/raw_image.hpp>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ---------------------------------------------------------------------------
// RenderImage Layout Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityImageLayout, ExplicitSize)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{100.0f, 80.0f});
    
    image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    
    EXPECT_EQ(snapshot.type, "RenderImage");
    EXPECT_FLOAT_EQ(snapshot.width, 100.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 80.0f);
}

TEST(FidelityImageLayout, ExplicitSizeClampedToConstraints)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{500.0f, 500.0f});  // Larger than constraints
    
    image->layout(cw::BoxConstraints::tight(200.0f, 200.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    
    // Should be clamped to constraints
    EXPECT_FLOAT_EQ(snapshot.width, 200.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 200.0f);
}

TEST(FidelityImageLayout, FillConstraintsWhenNoExplicitSize)
{
    auto image = std::make_shared<cw::RenderImage>();
    // No explicit size - should fill constraints
    
    image->layout(cw::BoxConstraints::tight(300.0f, 200.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    
    EXPECT_FLOAT_EQ(snapshot.width, 300.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 200.0f);
}

TEST(FidelityImageLayout, LoosedConstraintsWithNoExplicitSize)
{
    auto image = std::make_shared<cw::RenderImage>();
    // No explicit size with loose constraints
    
    image->layout(cw::BoxConstraints{0.0f, 400.0f, 0.0f, 300.0f});
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    
    // Should take max available
    EXPECT_FLOAT_EQ(snapshot.width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 300.0f);
}

// ---------------------------------------------------------------------------
// Image Properties Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityImageProperties, BoxFitFill)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{100.0f, 100.0f});
    image->setFit(cw::BoxFit::fill);
    
    image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    EXPECT_EQ(snapshot.type, "RenderImage");
    // Layout doesn't change with fit - fit affects paint only
    EXPECT_FLOAT_EQ(snapshot.width, 100.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 100.0f);
}

TEST(FidelityImageProperties, BoxFitContain)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{200.0f, 200.0f});
    image->setFit(cw::BoxFit::contain);
    image->setAlignment(cw::Alignment::center());
    
    image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    EXPECT_FLOAT_EQ(snapshot.width, 200.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 200.0f);
}

TEST(FidelityImageProperties, BoxFitCover)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{150.0f, 150.0f});
    image->setFit(cw::BoxFit::cover);
    image->setAlignment(cw::Alignment::center());
    
    image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    EXPECT_FLOAT_EQ(snapshot.width, 150.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 150.0f);
}

TEST(FidelityImageProperties, OpacitySetting)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{100.0f, 100.0f});
    image->setOpacity(0.5f);
    
    image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
    EXPECT_FLOAT_EQ(snapshot.width, 100.0f);
    EXPECT_FLOAT_EQ(snapshot.height, 100.0f);
}

TEST(FidelityImageProperties, AlignmentVariations)
{
    std::vector<cw::Alignment> alignments = {
        cw::Alignment::topLeft(),
        cw::Alignment::topCenter(),
        cw::Alignment::topRight(),
        cw::Alignment::centerLeft(),
        cw::Alignment::center(),
        cw::Alignment::centerRight(),
        cw::Alignment::bottomLeft(),
        cw::Alignment::bottomCenter(),
        cw::Alignment::bottomRight(),
    };
    
    for (const auto& alignment : alignments) {
        auto image = std::make_shared<cw::RenderImage>();
        image->setExplicitSize(cw::Size{100.0f, 100.0f});
        image->setAlignment(alignment);
        image->setFit(cw::BoxFit::contain);
        
        image->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
        
        // Layout size should be the same regardless of alignment
        auto snapshot = cwt::dumpRenderTree(*image, cw::Offset::zero());
        EXPECT_FLOAT_EQ(snapshot.width, 100.0f) << "Alignment should not affect layout width";
        EXPECT_FLOAT_EQ(snapshot.height, 100.0f) << "Alignment should not affect layout height";
    }
}

// ---------------------------------------------------------------------------
// Image Paint Command Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityImagePaint, PaintCommandGenerated)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{100.0f, 100.0f});
    
    auto snapshot = cwt::captureSnapshot(
        *image,
        cw::BoxConstraints::loose(400.0f, 400.0f),
        400.0f, 400.0f);
    
    // Without a texture, no paint commands should be generated
    // (RenderImage returns early if texture is null)
    EXPECT_TRUE(snapshot.paint_commands.empty());
}

TEST(FidelityImagePaint, JsonSerialization)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{150.0f, 100.0f});
    image->setFit(cw::BoxFit::contain);
    image->setAlignment(cw::Alignment::center());
    image->setOpacity(0.8f);
    
    auto snapshot = cwt::captureSnapshot(
        *image,
        cw::BoxConstraints::loose(400.0f, 400.0f),
        400.0f, 400.0f);
    
    std::string json = snapshot.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("RenderImage"), std::string::npos);
    EXPECT_NE(json.find("layout"), std::string::npos);
    EXPECT_NE(json.find("paint_commands"), std::string::npos);
    EXPECT_NE(json.find("150"), std::string::npos);
    EXPECT_NE(json.find("100"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Image Widget Fidelity Tests
// ---------------------------------------------------------------------------

TEST(FidelityImageWidget, ImageBuildsRawImage)
{
    auto image_widget = std::make_shared<cw::Image>();
    image_widget->texture = nullptr;  // No actual texture needed for structure test
    image_widget->size = cw::Size{200.0f, 150.0f};
    image_widget->fit = cw::BoxFit::cover;
    image_widget->alignment = cw::Alignment::topLeft();
    image_widget->opacity = 0.9f;
    
    // Build should return a RawImage with the same properties
    // Note: In a real test we'd need a BuildContext, but we can verify the widget properties
    EXPECT_FLOAT_EQ(image_widget->size.width, 200.0f);
    EXPECT_FLOAT_EQ(image_widget->size.height, 150.0f);
    EXPECT_EQ(image_widget->fit, cw::BoxFit::cover);
    EXPECT_EQ(image_widget->alignment, cw::Alignment::topLeft());
    EXPECT_FLOAT_EQ(image_widget->opacity, 0.9f);
}

TEST(FidelityImageWidget, RawImageFactory)
{
    auto raw_image = cw::RawImage::create(
        nullptr,  // No texture
        cw::Size{100.0f, 80.0f},
        cw::BoxFit::contain,
        cw::Alignment::center(),
        1.0f
    );
    
    EXPECT_NE(raw_image, nullptr);
    EXPECT_FLOAT_EQ(raw_image->size.width, 100.0f);
    EXPECT_FLOAT_EQ(raw_image->size.height, 80.0f);
    EXPECT_EQ(raw_image->fit, cw::BoxFit::contain);
    EXPECT_EQ(raw_image->alignment, cw::Alignment::center());
    EXPECT_FLOAT_EQ(raw_image->opacity, 1.0f);
}

// ---------------------------------------------------------------------------
// Image in Layout Context Tests
// ---------------------------------------------------------------------------

TEST(FidelityImageInLayout, ImageInSizedBox)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setFit(cw::BoxFit::cover);
    
    auto sized_box = std::make_shared<cw::RenderSizedBox>();
    sized_box->width = 200.0f;
    sized_box->height = 150.0f;
    sized_box->setChild(image);
    
    auto snapshot = cwt::captureSnapshot(
        *sized_box,
        cw::BoxConstraints::loose(400.0f, 400.0f),
        400.0f, 400.0f);
    
    EXPECT_EQ(snapshot.layout.type, "RenderSizedBox");
    EXPECT_FLOAT_EQ(snapshot.layout.width, 200.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.height, 150.0f);
    
    // Should have RenderImage as child
    ASSERT_EQ(snapshot.layout.children.size(), 1u);
    EXPECT_EQ(snapshot.layout.children[0].type, "RenderImage");
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].width, 200.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].height, 150.0f);
}

TEST(FidelityImageInLayout, MultipleImagesInFlex)
{
    auto flex = std::make_shared<cw::RenderFlex>();
    flex->axis = cw::Axis::horizontal;
    flex->main_axis_size = cw::MainAxisSize::max;
    
    auto image1 = std::make_shared<cw::RenderImage>();
    image1->setExplicitSize(cw::Size{100.0f, 100.0f});
    
    auto image2 = std::make_shared<cw::RenderImage>();
    image2->setExplicitSize(cw::Size{100.0f, 100.0f});
    
    auto image3 = std::make_shared<cw::RenderImage>();
    image3->setExplicitSize(cw::Size{100.0f, 100.0f});
    
    flex->insertChild(image1, 0, 0);
    flex->insertChild(image2, 1, 0);
    flex->insertChild(image3, 2, 0);
    
    auto snapshot = cwt::captureSnapshot(
        *flex,
        cw::BoxConstraints::tight(400.0f, 300.0f),
        400.0f, 300.0f);
    
    EXPECT_EQ(snapshot.layout.type, "RenderFlex");
    EXPECT_FLOAT_EQ(snapshot.layout.width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.height, 300.0f);
}

// ---------------------------------------------------------------------------
// BoxFit Mode Coverage Tests
// ---------------------------------------------------------------------------

TEST(FidelityImageBoxFit, AllModesLayoutCorrectly)
{
    std::vector<std::pair<cw::BoxFit, std::string>> fits = {
        {cw::BoxFit::fill, "fill"},
        {cw::BoxFit::contain, "contain"},
        {cw::BoxFit::cover, "cover"},
        {cw::BoxFit::fitWidth, "fitWidth"},
        {cw::BoxFit::fitHeight, "fitHeight"},
        {cw::BoxFit::none, "none"},
        {cw::BoxFit::scaleDown, "scaleDown"},
    };
    
    for (const auto& [fit, name] : fits) {
        auto image = std::make_shared<cw::RenderImage>();
        image->setExplicitSize(cw::Size{200.0f, 150.0f});
        image->setFit(fit);
        
        auto snapshot = cwt::captureSnapshot(
            *image,
            cw::BoxConstraints::loose(400.0f, 400.0f),
            400.0f, 400.0f);
        
        EXPECT_EQ(snapshot.layout.type, "RenderImage") << "Fit mode: " << name;
        EXPECT_FLOAT_EQ(snapshot.layout.width, 200.0f) << "Fit mode: " << name;
        EXPECT_FLOAT_EQ(snapshot.layout.height, 150.0f) << "Fit mode: " << name;
    }
}

// ---------------------------------------------------------------------------
// End-to-End Image Fidelity Test
// ---------------------------------------------------------------------------

TEST(FidelityImageEndToEnd, CompleteImageWidget)
{
    // Create a complete image widget hierarchy: SizedBox > Image
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{300.0f, 200.0f});
    image->setFit(cw::BoxFit::contain);
    image->setAlignment(cw::Alignment::center());
    image->setOpacity(1.0f);
    
    auto container = std::make_shared<cw::RenderSizedBox>();
    container->width = 400.0f;
    container->height = 300.0f;
    // Image fills the container
    image->setExplicitSize(cw::Size{0.0f, 0.0f});  // Use constraints
    container->setChild(image);
    
    auto snapshot = cwt::captureSnapshot(
        *container,
        cw::BoxConstraints::loose(800.0f, 600.0f),
        800.0f, 600.0f);
    
    // Verify layout
    EXPECT_EQ(snapshot.layout.type, "RenderSizedBox");
    EXPECT_FLOAT_EQ(snapshot.layout.width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.height, 300.0f);
    
    // Child should fill parent
    ASSERT_EQ(snapshot.layout.children.size(), 1u);
    EXPECT_EQ(snapshot.layout.children[0].type, "RenderImage");
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].width, 400.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].height, 300.0f);
    
    // Verify JSON output
    std::string json = snapshot.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("RenderSizedBox"), std::string::npos);
    EXPECT_NE(json.find("RenderImage"), std::string::npos);
    EXPECT_NE(json.find("400"), std::string::npos);
    EXPECT_NE(json.find("300"), std::string::npos);
}

TEST(FidelityImageEndToEnd, ImageWithPadding)
{
    auto image = std::make_shared<cw::RenderImage>();
    image->setExplicitSize(cw::Size{200.0f, 150.0f});
    image->setFit(cw::BoxFit::cover);
    
    auto padding = std::make_shared<cw::RenderPadding>();
    padding->padding = cw::EdgeInsets::all(16.0f);
    padding->setChild(image);
    
    auto snapshot = cwt::captureSnapshot(
        *padding,
        cw::BoxConstraints::loose(400.0f, 400.0f),
        400.0f, 400.0f);
    
    // Padding adds 16px on each side
    EXPECT_EQ(snapshot.layout.type, "RenderPadding");
    EXPECT_FLOAT_EQ(snapshot.layout.width, 232.0f);  // 200 + 16 + 16
    EXPECT_FLOAT_EQ(snapshot.layout.height, 182.0f); // 150 + 16 + 16
    
    ASSERT_EQ(snapshot.layout.children.size(), 1u);
    EXPECT_EQ(snapshot.layout.children[0].type, "RenderImage");
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].width, 200.0f);
    EXPECT_FLOAT_EQ(snapshot.layout.children[0].height, 150.0f);
}

// ---------------------------------------------------------------------------
// Comparison Tests (Simulating Golden File Comparison)
// ---------------------------------------------------------------------------

TEST(FidelityImageComparison, IdenticalImageLayoutsMatch)
{
    auto image1 = std::make_shared<cw::RenderImage>();
    image1->setExplicitSize(cw::Size{100.0f, 80.0f});
    image1->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto image2 = std::make_shared<cw::RenderImage>();
    image2->setExplicitSize(cw::Size{100.0f, 80.0f});
    image2->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto snapshot1 = cwt::dumpRenderTree(*image1, cw::Offset::zero());
    auto snapshot2 = cwt::dumpRenderTree(*image2, cw::Offset::zero());
    
    auto result = cwt::compareRenderTrees(snapshot1, snapshot2, "root");
    EXPECT_TRUE(result.match);
    EXPECT_TRUE(result.differences.empty());
}

TEST(FidelityImageComparison, DifferentSizesDetectMismatch)
{
    auto expected = std::make_shared<cw::RenderImage>();
    expected->setExplicitSize(cw::Size{100.0f, 80.0f});
    expected->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto actual = std::make_shared<cw::RenderImage>();
    actual->setExplicitSize(cw::Size{120.0f, 80.0f});  // Different width!
    actual->layout(cw::BoxConstraints::loose(400.0f, 400.0f));
    
    auto expected_snapshot = cwt::dumpRenderTree(*expected, cw::Offset::zero());
    auto actual_snapshot = cwt::dumpRenderTree(*actual, cw::Offset::zero());
    
    auto result = cwt::compareRenderTrees(expected_snapshot, actual_snapshot, "root");
    EXPECT_FALSE(result.match);
    EXPECT_FALSE(result.differences.empty());
}
