#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/ui/render_padding.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Padding::createRenderObject() const
    {
        auto ro    = std::make_shared<RenderPadding>();
        ro->padding = padding;
        return ro;
    }

    void Padding::updateRenderObject(RenderObject& ro) const
    {
        auto& rp   = static_cast<RenderPadding&>(ro);
        rp.padding = padding;
        rp.markNeedsLayout();
    }


    void Padding::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<DiagnosticProperty<EdgeInsets>>("padding", padding));
    }
} // namespace systems::leal::campello_widgets
