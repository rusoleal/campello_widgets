#include <campello_widgets/widgets/repaint_boundary.hpp>
#include <campello_widgets/ui/render_repaint_boundary.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RepaintBoundary::createRenderObject() const
    {
        return std::make_shared<RenderRepaintBoundary>();
    }

    void RepaintBoundary::updateRenderObject(RenderObject&) const
    {
        // No configurable properties — nothing to update.
    }

} // namespace systems::leal::campello_widgets
