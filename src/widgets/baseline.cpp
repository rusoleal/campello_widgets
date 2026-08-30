#include <campello_widgets/widgets/baseline.hpp>
#include <campello_widgets/ui/render_baseline.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Baseline::createRenderObject() const
    {
        auto r             = std::make_shared<RenderBaseline>();
        r->baseline        = baseline;
        r->baseline_type   = baseline_type;
        return r;
    }

    void Baseline::updateRenderObject(RenderObject& ro) const
    {
        auto& r          = static_cast<RenderBaseline&>(ro);
        r.baseline       = baseline;
        r.baseline_type  = baseline_type;
    }

} // namespace systems::leal::campello_widgets
