#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/transform.hpp>
#include <campello_widgets/ui/render_transform.hpp>

#include <cmath>

namespace systems::leal::campello_widgets
{

    // ----------------------------------------------------------------
    // Matrix helpers
    // ----------------------------------------------------------------

    Matrix4 Transform::rotation(float radians)
    {
        return RenderTransform::rotation(radians);
    }

    Matrix4 Transform::scaling(float s)
    {
        return RenderTransform::scaling(s);
    }

    Matrix4 Transform::scaling(float sx, float sy)
    {
        return RenderTransform::scaling(sx, sy);
    }

    Matrix4 Transform::translation(float dx, float dy)
    {
        return RenderTransform::translation(dx, dy);
    }

    // ----------------------------------------------------------------
    // RenderObjectWidget interface
    // ----------------------------------------------------------------

    std::shared_ptr<RenderObject> Transform::createRenderObject() const
    {
        auto ro = std::make_shared<RenderTransform>();
        ro->transform = transform;
        ro->alignment = alignment;
        return ro;
    }

    void Transform::updateRenderObject(RenderObject& ro) const
    {
        auto& rt = static_cast<RenderTransform&>(ro);
        rt.transform = transform;
        rt.alignment = alignment;
        rt.markNeedsPaint();
    }


    void Transform::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<StringProperty>("transform", "Matrix4(...)"));
    }
} // namespace systems::leal::campello_widgets
