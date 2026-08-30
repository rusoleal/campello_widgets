#include <campello_widgets/widgets/listener.hpp>
#include <campello_widgets/ui/render_listener.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Listener::createRenderObject() const
    {
        auto r               = std::make_shared<RenderListener>();
        r->on_pointer_down   = on_pointer_down;
        r->on_pointer_move   = on_pointer_move;
        r->on_pointer_up     = on_pointer_up;
        r->on_pointer_cancel = on_pointer_cancel;
        r->on_pointer_signal = on_pointer_signal;
        return r;
    }

    void Listener::updateRenderObject(RenderObject& ro) const
    {
        auto& r              = static_cast<RenderListener&>(ro);
        r.on_pointer_down    = on_pointer_down;
        r.on_pointer_move    = on_pointer_move;
        r.on_pointer_up      = on_pointer_up;
        r.on_pointer_cancel  = on_pointer_cancel;
        r.on_pointer_signal  = on_pointer_signal;
    }

} // namespace systems::leal::campello_widgets
