#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> ColoredBox::createRenderObject() const
    {
        auto ro   = std::make_shared<RenderColoredBox>();
        ro->color = color;
        return ro;
    }

    void ColoredBox::updateRenderObject(RenderObject& ro) const
    {
        auto& rcb = static_cast<RenderColoredBox&>(ro);
        rcb.color = color;
        rcb.markNeedsPaint();
    }

} // namespace systems::leal::campello_widgets
