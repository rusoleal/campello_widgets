#include <campello_widgets/widgets/ignore_pointer.hpp>
#include <campello_widgets/ui/render_ignore_pointer.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> IgnorePointer::createRenderObject() const
    {
        return std::make_shared<RenderIgnorePointer>(ignoring);
    }

    void IgnorePointer::updateRenderObject(RenderObject& ro) const
    {
        static_cast<RenderIgnorePointer&>(ro).setIgnoring(ignoring);
    }

} // namespace systems::leal::campello_widgets
