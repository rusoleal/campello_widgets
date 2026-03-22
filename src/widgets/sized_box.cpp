#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> SizedBox::createRenderObject() const
    {
        auto ro   = std::make_shared<RenderSizedBox>();
        ro->width  = width;
        ro->height = height;
        return ro;
    }

    void SizedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& rsb  = static_cast<RenderSizedBox&>(ro);
        rsb.width  = width;
        rsb.height = height;
        rsb.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
