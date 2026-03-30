#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/ui/render_clip_rrect.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ClipRRect::createRenderObject() const
    {
        auto ro = std::make_shared<RenderClipRRect>();
        ro->border_radius = border_radius;
        return ro;
    }

    void ClipRRect::updateRenderObject(RenderObject& ro) const
    {
        auto& rcrr = static_cast<RenderClipRRect&>(ro);
        rcrr.border_radius = border_radius;
        rcrr.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
