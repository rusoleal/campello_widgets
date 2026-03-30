#include <campello_widgets/widgets/clip_rect.hpp>
#include <campello_widgets/ui/render_clip_rect.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ClipRect::createRenderObject() const
    {
        return std::make_shared<RenderClipRect>();
    }

    void ClipRect::updateRenderObject(RenderObject&) const
    {
        // No configurable properties — nothing to update.
    }

} // namespace systems::leal::campello_widgets
