#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/render_object.hpp>

namespace systems::leal::campello_widgets
{

    void FocusNode::requestFocus()
    {
        if (auto* m = FocusManager::activeManager())
            m->requestFocus(this);
    }

    void FocusNode::unfocus()
    {
        if (auto* m = FocusManager::activeManager())
            m->unfocus(this);
    }

    FocusNode* FocusNode::parent() const noexcept
    {
        if (!owner_) return nullptr;
        for (RenderObject* p = owner_->parent(); p; p = p->parent())
        {
            if (auto* fn = p->ownedFocusNode())
                return fn;
        }
        return nullptr;
    }

} // namespace systems::leal::campello_widgets
