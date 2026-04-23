#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/ui/render_opacity.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Opacity::createRenderObject() const
    {
        return std::make_shared<RenderOpacity>(opacity);
    }

    void Opacity::updateRenderObject(RenderObject& ro) const
    {
        auto& render_opacity = static_cast<RenderOpacity&>(ro);
        render_opacity.setOpacity(opacity);
    }


    void Opacity::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<DoubleProperty>("opacity", opacity));
    }
} // namespace systems::leal::campello_widgets
