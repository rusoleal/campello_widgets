#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/ui/render_constrained_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ConstrainedBox::createRenderObject() const
    {
        auto ro = std::make_shared<RenderConstrainedBox>();
        ro->additional_constraints = additional_constraints;
        return ro;
    }

    void ConstrainedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& rcb = static_cast<RenderConstrainedBox&>(ro);
        rcb.additional_constraints = additional_constraints;
        rcb.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
