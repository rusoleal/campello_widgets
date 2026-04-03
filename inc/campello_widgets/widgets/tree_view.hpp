#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/tree_node.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;
    class ScrollController;

    /**
     * @brief Builder function type for tree rows.
     *
     * Called by TreeViewElement to build the widget for each visible row.
     * The builder receives the node, its depth, and expansion state.
     */
    using TreeRowBuilder = std::function<WidgetRef(
        BuildContext&,
        const TreeNode& node,
        int depth,
        bool is_expanded,
        bool has_children)>;

    /**
     * @brief A scrollable tree view with two-dimensional scrolling.
     *
     * TreeView displays a hierarchical tree structure where each node occupies
     * a row. The view scrolls vertically through rows and horizontally to
     * reveal deeply nested content based on indentation level.
     *
     * Features:
     * - Vertical scrolling through tree rows
     * - Horizontal scrolling for deep nesting (indentation)
     * - Lazy row building - only visible rows are mounted
     * - Expand/collapse state management via TreeController
     * - Configurable indentation and row height
     *
     * Usage:
     * @code
     * auto root = std::make_shared<TreeNode>();
     * root->content = Text::create("Root");
     *
     * auto child = std::make_shared<TreeNode>();
     * child->content = Text::create("Child");
     * root->children.push_back(child);
     *
     * auto controller = std::make_shared<TreeController>();
     * controller->expand(root.get());
     *
     * auto tree = std::make_shared<TreeView>();
     * tree->root = root;
     * tree->controller = controller;
     * tree->row_builder = [](BuildContext&, const TreeNode& node, int depth,
     *                        bool expanded, bool has_children) {
     *     return Row::create({
     *         .children = {
     *             SizedBox::create({.width = depth * 24.0f}),
     *             has_children ? Text::create(expanded ? "▼" : "▶")
     *                          : Text::create("  "),
     *             node.content,
     *         }
     *     });
     * };
     * @endcode
     */
    class TreeView : public RenderObjectWidget
    {
    public:
        /// Root node of the tree.
        std::shared_ptr<TreeNode> root;

        /// Controller managing expansion state. Creates internal if null.
        std::shared_ptr<TreeController> controller;

        /// Builder function for rows. Called lazily for visible rows only.
        TreeRowBuilder row_builder;

        /// Horizontal indentation per depth level in logical pixels.
        float indent_width = 24.0f;

        /// Fixed height for each row in logical pixels.
        float row_height = 48.0f;

        /// Optional controller for horizontal scrolling.
        std::shared_ptr<ScrollController> horizontal_controller;

        /// Optional controller for vertical scrolling.
        std::shared_ptr<ScrollController> vertical_controller;

        /// Scroll physics for momentum and boundary behavior.
        std::shared_ptr<ScrollPhysics> physics;

        std::shared_ptr<Element> createElement() const override;
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
