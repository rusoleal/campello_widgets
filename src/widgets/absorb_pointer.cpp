#include <campello_widgets/widgets/absorb_pointer.hpp>
#include <campello_widgets/ui/render_absorb_pointer.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> AbsorbPointer::createRenderObject() const
    {
        return std::make_shared<RenderAbsorbPointer>(absorbing);
    }

    void AbsorbPointer::updateRenderObject(RenderObject& ro) const
    {
        static_cast<RenderAbsorbPointer&>(ro).setAbsorbing(absorbing);
    }

} // namespace systems::leal::campello_widgets
