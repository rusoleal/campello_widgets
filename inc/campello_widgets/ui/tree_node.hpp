#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>
#include <campello_widgets/widgets/widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Immutable node in a tree structure.
     *
     * TreeNode stores the content widget and children of a tree node.
     * Expansion state is managed externally by TreeController, allowing
     * nodes to be reconstructed without losing UI state.
     *
     * The node owns its children via shared_ptr, forming a tree that
     * can be shared across multiple TreeView instances.
     */
    class TreeNode
    {
    public:
        /// Widget displayed for this node (typically a row with indentation + content).
        WidgetRef content;

        /// Child nodes. Empty for leaf nodes.
        std::vector<std::shared_ptr<TreeNode>> children;

        TreeNode() = default;
        explicit TreeNode(WidgetRef c) : content(std::move(c)) {}

        /// Returns true if this node has any children.
        bool hasChildren() const { return !children.empty(); }

        /// Returns the number of child nodes.
        size_t childCount() const { return children.size(); }
    };

    // Forward declaration
    class TreeController;

    /**
     * @brief Controller that manages tree expansion state.
     *
     * TreeController tracks which nodes are expanded/collapsed separately
     * from the TreeNode tree. This allows:
     * - Multiple TreeViews to share the same expansion state
     * - TreeNodes to be immutable and reconstructible
     * - Persistence of expansion state across tree rebuilds
     *
     * Similar to ScrollController, this can be created by the user and
     * passed to TreeView widgets, or the TreeView will create its own.
     */
    class TreeController
    {
    public:
        TreeController() = default;
        ~TreeController() = default;

        // Non-copyable - owns listener state
        TreeController(const TreeController&) = delete;
        TreeController& operator=(const TreeController&) = delete;

        // ------------------------------------------------------------------
        // Expansion state queries
        // ------------------------------------------------------------------

        /** @brief Returns true if the given node is expanded. */
        bool isExpanded(const TreeNode* node) const;

        /** @brief Returns true if the given node is expanded. Convenience overload. */
        bool isExpanded(const std::shared_ptr<TreeNode>& node) const;

        // ------------------------------------------------------------------
        // Expansion state modification
        // ------------------------------------------------------------------

        /** @brief Toggles the expansion state of the given node. */
        void toggleExpanded(const TreeNode* node);

        /** @brief Expands the given node. */
        void expand(const TreeNode* node);

        /** @brief Collapses the given node. */
        void collapse(const TreeNode* node);

        /** @brief Expands all nodes in the tree starting from root. */
        void expandAll(const TreeNode* root);

        /** @brief Collapses all nodes in the tree starting from root. */
        void collapseAll(const TreeNode* root);

        /** @brief Collapses all nodes except the given node and its ancestors. */
        void collapseAllExcept(const TreeNode* root, const TreeNode* keep_expanded);

        // ------------------------------------------------------------------
        // Listener API
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback that fires when any expansion state changes.
         * @return A listener ID for use with removeListener().
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Removes the listener with the given ID. */
        void removeListener(uint64_t id);

        /** @brief Notifies all listeners of a state change. */
        void notifyListeners();

    private:
        // Set of expanded node pointers
        std::unordered_set<const TreeNode*> expanded_nodes_;

        // Listeners
        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;
    };

} // namespace systems::leal::campello_widgets
