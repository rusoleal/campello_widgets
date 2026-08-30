#pragma once

#include <algorithm>
#include <vector>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Governs the order Tab/Shift+Tab visits a scope's traversal
     * candidates. Mirrors Flutter's FocusTraversalPolicy.
     *
     * `order()` receives the already-filtered candidate list for a single
     * scope (see FocusManager::nearestEnclosingScope()'s doc comment for
     * how that filtering works) and returns them in the order Tab should
     * visit them. `FocusManager::moveFocusForward()`/`moveFocusBackward()`
     * then just walk the returned list, wrapping at either end -- a
     * policy only decides ordering, not wrap-around behavior.
     */
    class FocusTraversalPolicy
    {
    public:
        virtual ~FocusTraversalPolicy() = default;
        virtual std::vector<FocusNode*> order(const std::vector<FocusNode*>& candidates) const = 0;
    };

    /**
     * @brief Registration order -- the order each node's Focus widget first
     * mounted. This is FocusManager's global default (assigned to any
     * scope that doesn't set its own `traversal_policy`), matching every
     * Tab flow already working in the app before FocusScope existed --
     * nothing regresses unless a scope explicitly opts into a different
     * policy.
     */
    class OrderedTraversalPolicy : public FocusTraversalPolicy
    {
    public:
        std::vector<FocusNode*> order(const std::vector<FocusNode*>& candidates) const override
        {
            return candidates;
        }
    };

    /**
     * @brief Approximate visual reading order: top-to-bottom, then
     * left-to-right within a row, using each node's last-painted
     * bounds() (see FocusNode::bounds()'s doc comment). Mirrors Flutter's
     * real default (ReadingOrderTraversalPolicy) -- available as an
     * explicit opt-in per scope (`scope_node->traversal_policy =
     * std::make_shared<ReadingOrderTraversalPolicy>();`), not applied
     * anywhere by default in this codebase yet.
     */
    class ReadingOrderTraversalPolicy : public FocusTraversalPolicy
    {
    public:
        std::vector<FocusNode*> order(const std::vector<FocusNode*>& candidates) const override
        {
            std::vector<FocusNode*> sorted = candidates;
            std::stable_sort(sorted.begin(), sorted.end(), [](FocusNode* a, FocusNode* b) {
                const Rect ra = a->bounds();
                const Rect rb = b->bounds();
                if (ra.y != rb.y) return ra.y < rb.y;
                return ra.x < rb.x;
            });
            return sorted;
        }
    };

} // namespace systems::leal::campello_widgets
