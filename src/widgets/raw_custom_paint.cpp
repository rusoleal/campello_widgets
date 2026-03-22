#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/ui/render_custom_paint.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RawCustomPaint::createRenderObject() const
    {
        auto ro = std::make_shared<RenderCustomPaint>();
        ro->setPainter(painter);
        return ro;
    }

    void RawCustomPaint::updateRenderObject(RenderObject& render_object) const
    {
        static_cast<RenderCustomPaint&>(render_object).setPainter(painter);
    }

} // namespace systems::leal::campello_widgets
