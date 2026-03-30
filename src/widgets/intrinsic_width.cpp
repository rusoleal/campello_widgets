#include <campello_widgets/widgets/intrinsic_width.hpp>
#include <campello_widgets/ui/render_intrinsic_width.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> IntrinsicWidth::createRenderObject() const
    {
        auto ro = std::make_shared<RenderIntrinsicWidth>();
        ro->step_width = step_width;
        return ro;
    }

    void IntrinsicWidth::updateRenderObject(RenderObject& ro) const
    {
        auto& riw = static_cast<RenderIntrinsicWidth&>(ro);
        riw.step_width = step_width;
        riw.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
