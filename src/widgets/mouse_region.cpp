#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/ui/render_mouse_region.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> MouseRegion::createRenderObject() const
    {
        auto r        = std::make_shared<RenderMouseRegion>();
        r->on_enter   = on_enter;
        r->on_exit    = on_exit;
        r->on_hover   = on_hover;
        r->cursor     = cursor;
        return r;
    }

    void MouseRegion::updateRenderObject(RenderObject& ro) const
    {
        auto& r       = static_cast<RenderMouseRegion&>(ro);
        r.on_enter    = on_enter;
        r.on_exit     = on_exit;
        r.on_hover    = on_hover;
        r.cursor      = cursor;
    }

} // namespace systems::leal::campello_widgets
