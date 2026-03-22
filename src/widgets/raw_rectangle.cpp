#include <campello_widgets/widgets/raw_rectangle.hpp>
#include <campello_widgets/ui/render_rectangle.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RawRectangle::createRenderObject() const
    {
        auto ro = std::make_shared<RenderRectangle>();
        ro->setColor(color);
        ro->setFill(fill);
        ro->setCornerRadius(corner_radius);
        return ro;
    }

    void RawRectangle::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro = static_cast<RenderRectangle&>(render_object);
        ro.setColor(color);
        ro.setFill(fill);
        ro.setCornerRadius(corner_radius);
    }

} // namespace systems::leal::campello_widgets
