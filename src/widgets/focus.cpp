#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Focus::createRenderObject() const
    {
        auto ro        = std::make_shared<RenderFocus>();
        ro->focus_node = focus_node;
        ro->auto_focus = auto_focus;

        if (focus_node)
        {
            if (auto* m = FocusManager::activeManager())
            {
                m->registerNode(focus_node.get());
                if (auto_focus)
                    m->requestFocus(focus_node.get());
            }
        }

        return ro;
    }

    void Focus::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro = static_cast<RenderFocus&>(render_object);

        // If the focus_node has changed, unregister the old and register the new.
        if (ro.focus_node != focus_node)
        {
            if (auto* m = FocusManager::activeManager())
            {
                if (ro.focus_node)
                    m->unregisterNode(ro.focus_node.get());
                if (focus_node)
                {
                    m->registerNode(focus_node.get());
                    if (auto_focus)
                        m->requestFocus(focus_node.get());
                }
            }
            ro.focus_node = focus_node;
        }

        ro.auto_focus = auto_focus;
    }

} // namespace systems::leal::campello_widgets
