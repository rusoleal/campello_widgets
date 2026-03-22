#include <campello_widgets/widgets/single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> SingleChildScrollView::createRenderObject() const
    {
        auto render = std::make_shared<RenderSingleChildScrollView>();
        render->scroll_axis = scroll_axis;
        render->setController(controller);
        render->setPhysics(physics);
        return render;
    }

    void SingleChildScrollView::updateRenderObject(RenderObject& render_object) const
    {
        auto& render = static_cast<RenderSingleChildScrollView&>(render_object);
        render.scroll_axis = scroll_axis;
        render.setController(controller);
        render.setPhysics(physics);
    }

} // namespace systems::leal::campello_widgets
