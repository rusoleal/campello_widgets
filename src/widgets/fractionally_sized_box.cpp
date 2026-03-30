#include <campello_widgets/widgets/fractionally_sized_box.hpp>
#include <campello_widgets/ui/render_fractionally_sized_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> FractionallySizedBox::createRenderObject() const
    {
        auto ro          = std::make_shared<RenderFractionallySizedBox>();
        ro->width_factor  = width_factor;
        ro->height_factor = height_factor;
        ro->alignment     = alignment;
        return ro;
    }

    void FractionallySizedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& rfsb        = static_cast<RenderFractionallySizedBox&>(ro);
        rfsb.width_factor  = width_factor;
        rfsb.height_factor = height_factor;
        rfsb.alignment     = alignment;
        rfsb.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
