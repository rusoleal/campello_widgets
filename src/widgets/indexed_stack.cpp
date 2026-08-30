#include <campello_widgets/widgets/indexed_stack.hpp>
#include <campello_widgets/ui/render_indexed_stack.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> IndexedStack::createRenderObject() const
    {
        auto r    = std::make_shared<RenderIndexedStack>();
        r->fit    = fit;
        r->index  = index;
        return r;
    }

    void IndexedStack::updateRenderObject(RenderObject& ro) const
    {
        auto& r  = static_cast<RenderIndexedStack&>(ro);
        if (r.fit == fit && r.index == index) return;
        r.fit    = fit;
        r.index  = index;
        r.markNeedsPaint();
    }

} // namespace systems::leal::campello_widgets
