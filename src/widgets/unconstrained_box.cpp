#include <campello_widgets/widgets/unconstrained_box.hpp>
#include <campello_widgets/ui/render_constraints_transform_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> UnconstrainedBox::createRenderObject() const
    {
        auto r = std::make_shared<RenderConstraintsTransformBox>();
        r->alignment = alignment;
        r->constraints_transform = [](const BoxConstraints&) { return BoxConstraints::unconstrained(); };
        return r;
    }

    void UnconstrainedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& r = static_cast<RenderConstraintsTransformBox&>(ro);
        r.alignment = alignment;
        r.constraints_transform = [](const BoxConstraints&) { return BoxConstraints::unconstrained(); };
    }

} // namespace systems::leal::campello_widgets
