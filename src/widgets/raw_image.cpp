#include <campello_widgets/widgets/raw_image.hpp>
#include <campello_widgets/ui/render_image.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RawImage::createRenderObject() const
    {
        std::cerr << "[RawImage] createRenderObject: texture=" << texture.get() 
                  << " size=" << size.width << "x" << size.height << "\n";
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
        std::cerr << "[RawImage] updateRenderObject: texture=" << texture.get() << "\n";
        auto& ro = static_cast<RenderImage&>(render_object);
        ro.setTexture(texture);
        ro.setExplicitSize(size);
        ro.setFit(fit);
        ro.setAlignment(alignment);
        ro.setOpacity(opacity);
    }

} // namespace systems::leal::campello_widgets
