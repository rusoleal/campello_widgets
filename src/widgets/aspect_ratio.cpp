#include <campello_widgets/widgets/aspect_ratio.hpp>
#include <campello_widgets/ui/render_aspect_ratio.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> AspectRatio::createRenderObject() const
    {
        auto ro = std::make_shared<RenderAspectRatio>();
        ro->aspect_ratio = aspect_ratio;
        return ro;
    }

    void AspectRatio::updateRenderObject(RenderObject& ro) const
    {
        auto& rar = static_cast<RenderAspectRatio&>(ro);
        rar.aspect_ratio = aspect_ratio;
        rar.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
