#include <campello_widgets/widgets/raw_image.hpp>
#include <campello_widgets/ui/render_image.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RawImage::createRenderObject() const
    {
        auto ro = std::make_shared<RenderImage>();
        ro->setTexture(texture);
        ro->setExplicitSize(size);
        ro->setFit(fit);
        ro->setAlignment(alignment);
        ro->setOpacity(opacity);
        return ro;
    }

    void RawImage::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro = static_cast<RenderImage&>(render_object);
        ro.setTexture(texture);
        ro.setExplicitSize(size);
        ro.setFit(fit);
        ro.setAlignment(alignment);
        ro.setOpacity(opacity);
    }

} // namespace systems::leal::campello_widgets
