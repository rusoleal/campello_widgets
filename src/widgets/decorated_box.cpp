#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/ui/render_decorated_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> DecoratedBox::createRenderObject() const
    {
        auto ro        = std::make_shared<RenderDecoratedBox>();
        ro->decoration = decoration;
        ro->position   = position;
        return ro;
    }

    void DecoratedBox::updateRenderObject(RenderObject& render_object) const
    {
        auto& rdb      = static_cast<RenderDecoratedBox&>(render_object);
        rdb.decoration = decoration;
        rdb.position   = position;
        rdb.markNeedsPaint();
    }


    void DecoratedBox::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<StringProperty>("decoration", "BoxDecoration(...)"));
    }
} // namespace systems::leal::campello_widgets
