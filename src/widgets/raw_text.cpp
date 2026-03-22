#include <campello_widgets/widgets/raw_text.hpp>
#include <campello_widgets/ui/render_text.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> RawText::createRenderObject() const
    {
        auto ro = std::make_shared<RenderText>();
        ro->setTextSpan(span);
        return ro;
    }

    void RawText::updateRenderObject(RenderObject& render_object) const
    {
        static_cast<RenderText&>(render_object).setTextSpan(span);
    }

} // namespace systems::leal::campello_widgets
