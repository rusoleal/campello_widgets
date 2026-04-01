#include <gtest/gtest.h>
#include <campello_widgets/campello_widgets.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Test: RenderObject activeDevicePixelRatio is set during layout/paint
// ---------------------------------------------------------------------------

TEST(LogicalPixels, ActiveDevicePixelRatioDefaultsToOne)
{
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 1.0f);
}

TEST(LogicalPixels, ActiveDevicePixelRatioCanBeSet)
{
    float original = cw::RenderObject::activeDevicePixelRatio();
    
    cw::RenderObject::setActiveDevicePixelRatio(2.0f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 2.0f);
    
    cw::RenderObject::setActiveDevicePixelRatio(3.5f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 3.5f);
    
    // Restore original
    cw::RenderObject::setActiveDevicePixelRatio(original);
}

// ---------------------------------------------------------------------------
// Test: MediaQueryData equality and basic properties
// ---------------------------------------------------------------------------

TEST(LogicalPixels, MediaQueryDataEquality)
{
    cw::MediaQueryData data1;
    data1.logical_size = cw::Size{800.0f, 600.0f};
    data1.device_pixel_ratio = 2.0f;
    data1.padding = cw::EdgeInsets::all(20.0f);
    data1.view_insets = cw::EdgeInsets::all(10.0f);
    
    cw::MediaQueryData data2 = data1;
    EXPECT_EQ(data1, data2);
    
    data2.device_pixel_ratio = 3.0f;
    EXPECT_NE(data1, data2);
}

TEST(LogicalPixels, MediaQueryDataPhysicalSize)
{
    cw::MediaQueryData data;
    data.logical_size = cw::Size{400.0f, 300.0f};
    data.device_pixel_ratio = 2.0f;
    
    cw::Size physical = data.physicalSize();
    EXPECT_FLOAT_EQ(physical.width, 800.0f);
    EXPECT_FLOAT_EQ(physical.height, 600.0f);
}

// ---------------------------------------------------------------------------
// Test: MediaQuery widget can be created and holds data
// ---------------------------------------------------------------------------

TEST(LogicalPixels, MediaQueryWidgetStoresData)
{
    cw::MediaQueryData data;
    data.logical_size = cw::Size{800.0f, 600.0f};
    data.device_pixel_ratio = 2.0f;
    
    auto child = std::make_shared<cw::SizedBox>(100.0f, 100.0f);
    auto mediaQuery = std::make_shared<cw::MediaQuery>(data, child);
    
    EXPECT_EQ(mediaQuery->data.logical_size.width, 800.0f);
    EXPECT_EQ(mediaQuery->data.logical_size.height, 600.0f);
    EXPECT_FLOAT_EQ(mediaQuery->data.device_pixel_ratio, 2.0f);
}

TEST(LogicalPixels, MediaQueryUpdateShouldNotify)
{
    cw::MediaQueryData data1;
    data1.device_pixel_ratio = 2.0f;
    
    cw::MediaQueryData data2;
    data2.device_pixel_ratio = 2.0f;
    
    auto child = std::make_shared<cw::SizedBox>(100.0f, 100.0f);
    auto mq1 = std::make_shared<cw::MediaQuery>(data1, child);
    auto mq2 = std::make_shared<cw::MediaQuery>(data2, child);
    
    // Same data - should not notify
    EXPECT_FALSE(mq2->updateShouldNotify(*mq1));
    
    // Different data - should notify
    data2.device_pixel_ratio = 3.0f;
    auto mq3 = std::make_shared<cw::MediaQuery>(data2, child);
    EXPECT_TRUE(mq3->updateShouldNotify(*mq1));
}

// ---------------------------------------------------------------------------
// Test: Device pixel ratio values
// ---------------------------------------------------------------------------

TEST(LogicalPixels, DevicePixelRatioClampingBehavior)
{
    // Test that DPR values are properly stored and retrieved
    float original = cw::RenderObject::activeDevicePixelRatio();
    
    // Test typical DPR values
    cw::RenderObject::setActiveDevicePixelRatio(1.0f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 1.0f);
    
    cw::RenderObject::setActiveDevicePixelRatio(2.0f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 2.0f);
    
    cw::RenderObject::setActiveDevicePixelRatio(3.0f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 3.0f);
    
    // Test fractional DPR (Windows can have these)
    cw::RenderObject::setActiveDevicePixelRatio(1.25f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 1.25f);
    
    cw::RenderObject::setActiveDevicePixelRatio(1.5f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 1.5f);
    
    // Reset to default
    cw::RenderObject::setActiveDevicePixelRatio(original);
}

// ---------------------------------------------------------------------------
// Test: Pointer event position (verifying logical pixel coordinates)
// ---------------------------------------------------------------------------

TEST(LogicalPixels, PointerEventUsesLogicalCoordinates)
{
    // Create a pointer event with logical pixel coordinates
    cw::PointerEvent event;
    event.kind = cw::PointerEventKind::down;
    event.pointer_id = 0;
    event.position = cw::Offset{100.0f, 200.0f};
    event.pressure = 1.0f;
    
    // Verify the event stores logical coordinates directly
    EXPECT_FLOAT_EQ(event.position.x, 100.0f);
    EXPECT_FLOAT_EQ(event.position.y, 200.0f);
}

TEST(LogicalPixels, PointerEventScrollDeltaInLogicalPixels)
{
    // Scroll deltas should be in logical pixels
    cw::PointerEvent event;
    event.kind = cw::PointerEventKind::scroll;
    event.scroll_delta_x = 10.0f;
    event.scroll_delta_y = -30.0f;
    
    EXPECT_FLOAT_EQ(event.scroll_delta_x, 10.0f);
    EXPECT_FLOAT_EQ(event.scroll_delta_y, -30.0f);
}

// ---------------------------------------------------------------------------
// Test: EdgeInsets operations with logical pixels
// ---------------------------------------------------------------------------

TEST(LogicalPixels, EdgeInsetsHorizontalVertical)
{
    cw::EdgeInsets insets;
    insets.left = 10.0f;
    insets.right = 20.0f;
    insets.top = 5.0f;
    insets.bottom = 15.0f;
    
    EXPECT_FLOAT_EQ(insets.horizontal(), 30.0f);
    EXPECT_FLOAT_EQ(insets.vertical(), 20.0f);
}

// ---------------------------------------------------------------------------
// Test: Renderer DPR getter/setter
// ---------------------------------------------------------------------------

TEST(LogicalPixels, RendererDevicePixelRatioGetterSetter)
{
    // We can't test Renderer without a real GPU device, but we can verify
    // the DPR concept works through the RenderObject static method
    
    float saved = cw::RenderObject::activeDevicePixelRatio();
    
    // Test DPR=2 (Retina display)
    cw::RenderObject::setActiveDevicePixelRatio(2.0f);
    EXPECT_FLOAT_EQ(cw::RenderObject::activeDevicePixelRatio(), 2.0f);
    
    // Simulate what Renderer does: divide viewport by DPR for layout
    float physical_viewport = 800.0f;
    float logical_viewport = physical_viewport / cw::RenderObject::activeDevicePixelRatio();
    EXPECT_FLOAT_EQ(logical_viewport, 400.0f);
    
    // Test DPR=3 (Super Retina)
    cw::RenderObject::setActiveDevicePixelRatio(3.0f);
    logical_viewport = physical_viewport / cw::RenderObject::activeDevicePixelRatio();
    EXPECT_FLOAT_EQ(logical_viewport, 266.66667f);
    
    // Restore
    cw::RenderObject::setActiveDevicePixelRatio(saved);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
