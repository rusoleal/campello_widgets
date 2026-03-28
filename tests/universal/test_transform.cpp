#include <gtest/gtest.h>
#include <campello_widgets/widgets/transform.hpp>
#include <campello_widgets/ui/render_transform.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

#include <cmath>

namespace cw = systems::leal::campello_widgets;

// ============================================================================
// RenderTransform Tests - Layout
// ============================================================================

TEST(RenderTransform, LayoutPassesThroughChildSize)
{
    // Transform is layout-transparent: child's size passes through unchanged
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 100.0f;
    child->height = 50.0f;

    cw::RenderTransform transform;
    transform.setChild(child);
    transform.layout(cw::BoxConstraints::loose(300.0f, 300.0f));

    // Transform should report child's size (layout is transparent)
    EXPECT_FLOAT_EQ(transform.size().width,  100.0f);
    EXPECT_FLOAT_EQ(transform.size().height, 50.0f);
    EXPECT_FLOAT_EQ(child->size().width,   100.0f);
    EXPECT_FLOAT_EQ(child->size().height,  50.0f);
}

TEST(RenderTransform, LayoutWithTightConstraints)
{
    // Child without explicit size - will use constraints
    auto child = std::make_shared<cw::RenderSizedBox>();

    cw::RenderTransform transform;
    transform.setChild(child);
    transform.layout(cw::BoxConstraints::tight(200.0f, 150.0f));

    // Child gets tight constraints, transform reports constrained size
    EXPECT_FLOAT_EQ(child->size().width,  200.0f);
    EXPECT_FLOAT_EQ(child->size().height, 150.0f);
    EXPECT_FLOAT_EQ(transform.size().width,  200.0f);
    EXPECT_FLOAT_EQ(transform.size().height, 150.0f);
}

TEST(RenderTransform, LayoutWithoutChild)
{
    cw::RenderTransform transform;
    transform.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    // Without child, size is zero (constrained)
    EXPECT_FLOAT_EQ(transform.size().width,  0.0f);
    EXPECT_FLOAT_EQ(transform.size().height, 0.0f);
}

TEST(RenderTransform, LayoutWithZeroConstraintsNoChild)
{
    cw::RenderTransform transform;
    transform.layout(cw::BoxConstraints::tight(0.0f, 0.0f));

    EXPECT_FLOAT_EQ(transform.size().width,  0.0f);
    EXPECT_FLOAT_EQ(transform.size().height, 0.0f);
}

// ============================================================================
// RenderTransform Tests - Matrix Helpers
// ============================================================================

TEST(RenderTransform, RotationMatrix)
{
    const float angle = 0.5f; // radians
    auto mat = cw::RenderTransform::rotation(angle);
    
    float c = std::cos(angle);
    float s = std::sin(angle);
    
    // Check rotation matrix structure
    EXPECT_FLOAT_EQ(mat.data[0], c);   // cos(angle)
    EXPECT_FLOAT_EQ(mat.data[1], s);   // sin(angle)
    EXPECT_FLOAT_EQ(mat.data[4], -s);  // -sin(angle)
    EXPECT_FLOAT_EQ(mat.data[5], c);   // cos(angle)
    EXPECT_FLOAT_EQ(mat.data[10], 1.0f); // Z unchanged
    EXPECT_FLOAT_EQ(mat.data[15], 1.0f); // W component
}

TEST(RenderTransform, UniformScalingMatrix)
{
    auto mat = cw::RenderTransform::scaling(2.0f);
    
    EXPECT_FLOAT_EQ(mat.data[0], 2.0f);  // X scale
    EXPECT_FLOAT_EQ(mat.data[5], 2.0f);  // Y scale
    EXPECT_FLOAT_EQ(mat.data[10], 1.0f); // Z unchanged
    EXPECT_FLOAT_EQ(mat.data[15], 1.0f); // W component
}

TEST(RenderTransform, NonUniformScalingMatrix)
{
    auto mat = cw::RenderTransform::scaling(2.0f, 3.0f);
    
    EXPECT_FLOAT_EQ(mat.data[0], 2.0f);  // X scale
    EXPECT_FLOAT_EQ(mat.data[5], 3.0f);  // Y scale
    EXPECT_FLOAT_EQ(mat.data[10], 1.0f); // Z unchanged
    EXPECT_FLOAT_EQ(mat.data[15], 1.0f); // W component
}

TEST(RenderTransform, TranslationMatrix)
{
    auto mat = cw::RenderTransform::translation(100.0f, 50.0f);
    
    // Matrix is row-major; translation is in the last column (indices 3, 7, 11)
    EXPECT_FLOAT_EQ(mat.data[3], 100.0f);  // X translation
    EXPECT_FLOAT_EQ(mat.data[7], 50.0f);   // Y translation
    EXPECT_FLOAT_EQ(mat.data[11], 0.0f);   // Z translation
}

TEST(RenderTransform, IdentityTransform)
{
    cw::RenderTransform transform;
    // Default transform should be identity
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_FLOAT_EQ(transform.transform.data[i * 4 + j], expected);
        }
    }
}

// ============================================================================
// Transform Widget Tests - Creation and Update
// ============================================================================

TEST(TransformWidget, CreateRenderObject)
{
    auto widget = std::make_shared<cw::Transform>();
    widget->transform = cw::Transform::rotation(0.5f);
    widget->alignment = cw::Alignment::topLeft();
    
    auto ro = widget->createRenderObject();
    ASSERT_NE(ro, nullptr);
    
    auto* rt = dynamic_cast<cw::RenderTransform*>(ro.get());
    ASSERT_NE(rt, nullptr);
    
    // Verify properties were transferred
    EXPECT_FLOAT_EQ(rt->transform.data[0], std::cos(0.5f));
    EXPECT_FLOAT_EQ(rt->alignment.x, -1.0f); // topLeft
    EXPECT_FLOAT_EQ(rt->alignment.y, -1.0f);
}

TEST(TransformWidget, UpdateRenderObject)
{
    auto widget = std::make_shared<cw::Transform>();
    widget->transform = cw::Transform::scaling(2.0f);
    widget->alignment = cw::Alignment::center();
    
    // Create initial render object
    auto ro = widget->createRenderObject();
    auto* rt = static_cast<cw::RenderTransform*>(ro.get());
    
    // Change widget properties
    widget->transform = cw::Transform::scaling(3.0f);
    widget->alignment = cw::Alignment::bottomRight();
    
    // Update render object
    widget->updateRenderObject(*ro);
    
    // Verify properties were updated
    EXPECT_FLOAT_EQ(rt->transform.data[0], 3.0f);
    EXPECT_FLOAT_EQ(rt->alignment.x, 1.0f); // bottomRight
    EXPECT_FLOAT_EQ(rt->alignment.y, 1.0f);
}

// ============================================================================
// Transform Widget Tests - Static Factory Methods
// ============================================================================

TEST(TransformWidget, FactoryRotate)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    auto transform = cw::Transform::rotate(0.75f);
    
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->alignment.x, 0.0f); // center
    EXPECT_FLOAT_EQ(transform->alignment.y, 0.0f);
    
    // Verify rotation matrix
    float c = std::cos(0.75f);
    float s = std::sin(0.75f);
    EXPECT_FLOAT_EQ(transform->transform.data[0], c);
    EXPECT_FLOAT_EQ(transform->transform.data[1], s);
    EXPECT_FLOAT_EQ(transform->transform.data[4], -s);
    EXPECT_FLOAT_EQ(transform->transform.data[5], c);
}

TEST(TransformWidget, FactoryScaleUniform)
{
    auto transform = cw::Transform::scale(2.5f);
    
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->alignment.x, 0.0f); // center
    EXPECT_FLOAT_EQ(transform->alignment.y, 0.0f);
    EXPECT_FLOAT_EQ(transform->transform.data[0], 2.5f);
    EXPECT_FLOAT_EQ(transform->transform.data[5], 2.5f);
}

TEST(TransformWidget, FactoryScaleNonUniform)
{
    auto transform = cw::Transform::scale(2.0f, 3.0f);
    
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->alignment.x, 0.0f); // center
    EXPECT_FLOAT_EQ(transform->alignment.y, 0.0f);
    EXPECT_FLOAT_EQ(transform->transform.data[0], 2.0f);
    EXPECT_FLOAT_EQ(transform->transform.data[5], 3.0f);
}

TEST(TransformWidget, FactoryTranslate)
{
    auto transform = cw::Transform::translate(100.0f, 200.0f);
    
    ASSERT_NE(transform, nullptr);
    // Translation should use topLeft alignment (no pivot adjustment)
    EXPECT_FLOAT_EQ(transform->alignment.x, -1.0f); // topLeft
    EXPECT_FLOAT_EQ(transform->alignment.y, -1.0f);
    // Matrix is row-major; translation is at indices 3, 7
    EXPECT_FLOAT_EQ(transform->transform.data[3], 100.0f);
    EXPECT_FLOAT_EQ(transform->transform.data[7], 200.0f);
}

// ============================================================================
// Transform Widget Tests - Matrix Helpers
// ============================================================================

TEST(TransformWidget, RotationHelper)
{
    auto mat = cw::Transform::rotation(0.3f);
    
    float c = std::cos(0.3f);
    float s = std::sin(0.3f);
    
    EXPECT_FLOAT_EQ(mat.data[0], c);
    EXPECT_FLOAT_EQ(mat.data[1], s);
    EXPECT_FLOAT_EQ(mat.data[4], -s);
    EXPECT_FLOAT_EQ(mat.data[5], c);
}

TEST(TransformWidget, ScalingUniformHelper)
{
    auto mat = cw::Transform::scaling(1.5f);
    
    EXPECT_FLOAT_EQ(mat.data[0], 1.5f);
    EXPECT_FLOAT_EQ(mat.data[5], 1.5f);
}

TEST(TransformWidget, ScalingNonUniformHelper)
{
    auto mat = cw::Transform::scaling(2.0f, 4.0f);
    
    EXPECT_FLOAT_EQ(mat.data[0], 2.0f);
    EXPECT_FLOAT_EQ(mat.data[5], 4.0f);
}

TEST(TransformWidget, TranslationHelper)
{
    auto mat = cw::Transform::translation(50.0f, 75.0f);
    
    // Matrix is row-major; translation is at indices 3, 7
    EXPECT_FLOAT_EQ(mat.data[3], 50.0f);
    EXPECT_FLOAT_EQ(mat.data[7], 75.0f);
}

// ============================================================================
// Transform Widget Tests - Construction
// ============================================================================

TEST(TransformWidget, DefaultConstruction)
{
    cw::Transform transform;
    
    // Default should be identity with center alignment
    EXPECT_FLOAT_EQ(transform.alignment.x, 0.0f);
    EXPECT_FLOAT_EQ(transform.alignment.y, 0.0f);
    
    // Check identity matrix
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_FLOAT_EQ(transform.transform.data[i * 4 + j], expected);
        }
    }
}

TEST(TransformWidget, ExplicitConstruction)
{
    auto mat = cw::Transform::rotation(0.5f);
    auto child_ro = std::make_shared<cw::RenderSizedBox>();
    
    // Note: The constructor takes a WidgetRef (shared_ptr<Widget>), not RenderObject
    // We can only test with nullptr since we don't have a test widget handy
    cw::Transform transform(mat, cw::Alignment::topRight(), nullptr);
    
    EXPECT_FLOAT_EQ(transform.alignment.x, 1.0f);  // topRight
    EXPECT_FLOAT_EQ(transform.alignment.y, -1.0f);
    EXPECT_EQ(transform.child, nullptr);
    
    // Verify matrix was copied
    EXPECT_FLOAT_EQ(transform.transform.data[0], mat.data[0]);
}

// ============================================================================
// Integration Test - Full widget tree simulation
// ============================================================================

TEST(TransformIntegration, WidgetCreatesCorrectRenderObject)
{
    // Build a transform widget with a sized child
    auto widget = std::make_shared<cw::Transform>();
    widget->transform = cw::Transform::rotation(0.25f);
    widget->alignment = cw::Alignment::center();
    
    // Create the render object
    auto ro = widget->createRenderObject();
    auto* rt = static_cast<cw::RenderTransform*>(ro.get());
    
    // Set up a child render box (simulating what the element would do)
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width = 100.0f;
    child->height = 50.0f;
    rt->setChild(child);
    
    // Layout
    rt->layout(cw::BoxConstraints::loose(500.0f, 500.0f));
    
    // Verify layout is transparent
    EXPECT_FLOAT_EQ(rt->size().width, 100.0f);
    EXPECT_FLOAT_EQ(rt->size().height, 50.0f);
}
