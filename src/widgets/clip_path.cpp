#include <campello_widgets/widgets/clip_path.hpp>
#include <campello_widgets/ui/render_clip_path.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ClipPath::createRenderObject() const
    {
        auto ro = std::make_shared<RenderClipPath>();
        ro->clip_path_builder = clip_path_builder;
        return ro;
    }

    void ClipPath::updateRenderObject(RenderObject& ro) const
    {
        auto& rcp = static_cast<RenderClipPath&>(ro);
        rcp.clip_path_builder = clip_path_builder;
        rcp.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
