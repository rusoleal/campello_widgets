#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/ui/render_padding.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Padding::createRenderObject() const
    {
        auto ro    = std::make_shared<RenderPadding>();
        ro->padding = padding;
        return ro;
    }

    void Padding::updateRenderObject(RenderObject& ro) const
    {
        auto& rp   = static_cast<RenderPadding&>(ro);
        rp.padding = padding;
        rp.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
