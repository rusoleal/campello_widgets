#include <campello_widgets/widgets/flow.hpp>
#include <campello_widgets/ui/render_flow.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Flow::createRenderObject() const
    {
        auto r      = std::make_shared<RenderFlow>();
        r->delegate = delegate;
        return r;
    }

    void Flow::updateRenderObject(RenderObject& ro) const
    {
        static_cast<RenderFlow&>(ro).delegate = delegate;
    }

    void Flow::insertRenderObjectChild(
        RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const
    {
        static_cast<RenderFlow&>(parent).insertChild(std::move(child_box), index);
    }

    void Flow::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderFlow&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
