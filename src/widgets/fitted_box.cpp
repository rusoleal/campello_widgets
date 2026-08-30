#include <campello_widgets/widgets/fitted_box.hpp>
#include <campello_widgets/ui/render_fitted_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> FittedBox::createRenderObject() const
    {
        auto r        = std::make_shared<RenderFittedBox>();
        r->fit        = fit;
        r->alignment  = alignment;
        return r;
    }

    void FittedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& r      = static_cast<RenderFittedBox&>(ro);
        r.fit        = fit;
        r.alignment  = alignment;
    }

} // namespace systems::leal::campello_widgets
