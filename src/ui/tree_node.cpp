#include <campello_widgets/ui/tree_node.hpp>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Expansion state queries
    // -------------------------------------------------------------------------

    bool TreeController::isExpanded(const TreeNode* node) const
    {
        if (!this || !node) return false;
        return expanded_nodes_.count(node) > 0;
    }

    bool TreeController::isExpanded(const std::shared_ptr<TreeNode>& node) const
    {
        return isExpanded(node.get());
    }

    // -------------------------------------------------------------------------
    // Expansion state modification
    // -------------------------------------------------------------------------

    void TreeController::toggleExpanded(const TreeNode* node)
    {
        if (!this || !node) return;

        if (isExpanded(node))
            collapse(node);
        else
            expand(node);
    }

    void TreeController::expand(const TreeNode* node)
    {
        if (!this || !node || !node->hasChildren()) return;

        bool changed = expanded_nodes_.insert(node).second;
        if (changed)
            notifyListeners();
    }

    void TreeController::collapse(const TreeNode* node)
    {
        if (!this || !node) return;

        bool changed = expanded_nodes_.erase(node) > 0;
        if (changed)
            notifyListeners();
    }

    void TreeController::expandAll(const TreeNode* root)
    {
        if (!root) return;

        bool changed = false;

        // Depth-first expansion
        std::vector<const TreeNode*> stack;
        stack.push_back(root);

        while (!stack.empty())
        {
            const TreeNode* node = stack.back();
            stack.pop_back();

            if (node->hasChildren())
            {
                changed |= expanded_nodes_.insert(node).second;

                for (const auto& child : node->children)
                {
                    if (child)
                        stack.push_back(child.get());
                }
            }
        }

        if (changed)
            notifyListeners();
    }

    void TreeController::collapseAll(const TreeNode* root)
    {
        if (!root) return;

        bool changed = false;

        // Depth-first collapse
        std::vector<const TreeNode*> stack;
        stack.push_back(root);

        while (!stack.empty())
        {
            const TreeNode* node = stack.back();
            stack.pop_back();

            changed |= expanded_nodes_.erase(node) > 0;

            for (const auto& child : node->children)
            {
                if (child)
                    stack.push_back(child.get());
            }
        }

        if (changed)
            notifyListeners();
    }

    void TreeController::collapseAllExcept(const TreeNode* root, const TreeNode* keep_expanded)
    {
        if (!root) return;

        // Build set of nodes to keep expanded (keep_expanded and its ancestors)
        std::unordered_set<const TreeNode*> keep_set;
        if (keep_expanded)
        {
            keep_set.insert(keep_expanded);

            // Find ancestors by walking from root
            std::vector<const TreeNode*> stack;
            stack.push_back(root);

            while (!stack.empty())
            {
                const TreeNode* node = stack.back();
                stack.pop_back();

                for (const auto& child : node->children)
                {
                    if (!child) continue;

                    if (child.get() == keep_expanded || keep_set.count(child.get()))
                    {
                        keep_set.insert(node);
                    }
                    stack.push_back(child.get());
                }
            }
        }

        // Collapse all nodes not in keep_set
        bool changed = false;
        std::vector<const TreeNode*> to_remove;

        for (const TreeNode* node : expanded_nodes_)
        {
            if (!keep_set.count(node))
                to_remove.push_back(node);
        }

        for (const TreeNode* node : to_remove)
        {
            expanded_nodes_.erase(node);
            changed = true;
        }

        if (changed)
            notifyListeners();
    }

    // -------------------------------------------------------------------------
    // Listener API
    // -------------------------------------------------------------------------

    uint64_t TreeController::addListener(std::function<void()> fn)
    {
        uint64_t id = next_listener_id_++;
        listeners_.push_back({id, std::move(fn)});
        return id;
    }

    void TreeController::removeListener(uint64_t id)
    {
        auto it = std::remove_if(listeners_.begin(), listeners_.end(),
            [id](const auto& pair) { return pair.first == id; });
        listeners_.erase(it, listeners_.end());
    }

    void TreeController::notifyListeners()
    {
        // Copy to avoid issues if callbacks modify listeners
        auto copy = listeners_;
        for (auto& [id, fn] : copy)
        {
            if (fn) fn();
        }
    }

} // namespace systems::leal::campello_widgets
