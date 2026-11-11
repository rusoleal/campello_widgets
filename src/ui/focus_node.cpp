#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/render_object.hpp>

namespace systems::leal::campello_widgets
{

    // A node owned by a render object belongs to that object's window
    // (Phase 17); an unowned node uses the ambient/process-wide manager.
    static FocusManager* managerFor(const RenderObject* owner) noexcept
    {
        if (owner && RenderObject::isAlive(owner)) return owner->focusManager();
        return FocusManager::activeManager();
    }

    void FocusNode::requestFocus()
    {
        if (auto* m = managerFor(owner_))
            m->requestFocus(this);
    }

    void FocusNode::unfocus()
    {
        if (auto* m = managerFor(owner_))
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
