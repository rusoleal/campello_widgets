#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

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

} // namespace systems::leal::campello_widgets
