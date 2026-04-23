#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/ui/render_align.hpp>

#include <sstream>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Align::createRenderObject() const
    {
        auto ro          = std::make_shared<RenderAlign>();
        ro->alignment    = alignment;
        ro->width_factor  = width_factor;
        ro->height_factor = height_factor;
        return ro;
    }

    void Align::updateRenderObject(RenderObject& ro) const
    {
        auto& ra          = static_cast<RenderAlign&>(ro);
        ra.alignment      = alignment;
        ra.width_factor   = width_factor;
        ra.height_factor  = height_factor;
        ra.markNeedsLayout();
    }


    void Align::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        std::ostringstream oss;
        oss << "Alignment(" << alignment.x << ", " << alignment.y << ")";
        properties.add(std::make_unique<StringProperty>("alignment", oss.str()));
    }
} // namespace systems::leal::campello_widgets
