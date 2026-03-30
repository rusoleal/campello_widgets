#include <campello_widgets/widgets/clip_oval.hpp>
#include <campello_widgets/ui/render_clip_oval.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ClipOval::createRenderObject() const
    {
        return std::make_shared<RenderClipOval>();
    }

    void ClipOval::updateRenderObject(RenderObject&) const
    {
        // No configurable properties — nothing to update.
    }

} // namespace systems::leal::campello_widgets
