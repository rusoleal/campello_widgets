#include <campello_widgets/widgets/intrinsic_height.hpp>
#include <campello_widgets/ui/render_intrinsic_height.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> IntrinsicHeight::createRenderObject() const
    {
        auto ro = std::make_shared<RenderIntrinsicHeight>();
        ro->step_height = step_height;
        return ro;
    }

    void IntrinsicHeight::updateRenderObject(RenderObject& ro) const
    {
        auto& rih = static_cast<RenderIntrinsicHeight&>(ro);
        rih.step_height = step_height;
        rih.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
