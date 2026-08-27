#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/offstage.hpp>
#include <campello_widgets/ui/render_offstage.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Offstage::createRenderObject() const
    {
        return std::make_shared<RenderOffstage>(offstage);
    }

    void Offstage::updateRenderObject(RenderObject& ro) const
    {
        auto& render_offstage = static_cast<RenderOffstage&>(ro);
        render_offstage.setOffstage(offstage);
    }

    void Offstage::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<BoolProperty>("offstage", offstage));
    }

} // namespace systems::leal::campello_widgets
