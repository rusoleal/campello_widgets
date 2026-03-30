#include <campello_widgets/widgets/wrap.hpp>
#include <campello_widgets/ui/render_wrap.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Wrap::createRenderObject() const
    {
        auto ro = std::make_shared<RenderWrap>();
        ro->direction            = direction;
        ro->alignment            = alignment;
        ro->spacing              = spacing;
        ro->run_alignment        = run_alignment;
        ro->run_spacing          = run_spacing;
        ro->cross_axis_alignment = cross_axis_alignment;
        return ro;
    }

    void Wrap::updateRenderObject(RenderObject& ro) const
    {
        auto& rw = static_cast<RenderWrap&>(ro);
        rw.direction            = direction;
        rw.alignment            = alignment;
        rw.spacing              = spacing;
        rw.run_alignment        = run_alignment;
        rw.run_spacing          = run_spacing;
        rw.cross_axis_alignment = cross_axis_alignment;
        rw.markNeedsLayout();
    }

    void Wrap::insertRenderObjectChild(
        RenderObject& parent,
        std::shared_ptr<RenderBox> child_box,
        int index) const
    {
        static_cast<RenderWrap&>(parent).insertChild(std::move(child_box), index);
    }

    void Wrap::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderWrap&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
