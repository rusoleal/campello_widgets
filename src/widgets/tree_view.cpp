#include <algorithm>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/tree_view.hpp>
#include <campello_widgets/ui/render_tree_view.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // TreeViewElement - builds only rows in the current visible range
    // =========================================================================

    class TreeViewElement : public RenderObjectElement
    {
    public:
        explicit TreeViewElement(std::shared_ptr<const TreeView> widget)
            : RenderObjectElement(std::move(widget))
        {}

        void mount(Element* parent) override
        {
            RenderObjectElement::mount(parent);
            wireCallback();
            wireController();
        }

        void unmount() override
        {
            // Unmount all row elements
            for (auto& [idx, elem] : row_elements_)
            {
                if (elem) elem->unmount();
            }
            row_elements_.clear();

            // Remove controller listener
            if (auto* rv = dynamic_cast<RenderTreeView*>(render_object_.get()))
            {
                rv->on_visible_range_changed = nullptr;
            }

            if (controller_listener_id_ != 0)
            {
                const auto& w = static_cast<const TreeView&>(*widget_);
                if (w.controller)
                    w.controller->removeListener(controller_listener_id_);
            }

            RenderObjectElement::unmount();
        }

        void update(WidgetRef new_widget) override
        {
            // Unmount all currently mounted rows so they get rebuilt with the
            // updated row_builder. This is necessary because performBuild()
            // skips already-mounted rows, which means selection changes (or
            // any other row_builder update) would not be reflected visually.
            auto& rv = renderTreeView();
            for (auto& [idx, elem] : row_elements_)
            {
                if (elem) elem->unmount();
                rv.removeRowBox(idx);
            }
            row_elements_.clear();
            last_total_rows_ = -1;

            RenderObjectElement::update(std::move(new_widget));
            wireCallback();
            wireController();
            rebuild();
        }

    protected:
        void performBuild() override
        {
            const auto& w = static_cast<const TreeView&>(*widget_);
            auto& rv = renderTreeView();

            if (!w.row_builder || !w.root)
                return;

            // Get current visible range with buffer
            auto visible = rv.visibleRange();
            if (visible.last_row < visible.first_row)
                return;

            // Add buffer
            const int row_buffer = 2;
            int total_rows = rv.computeTotalRows();

            // If tree structure changed (expansion/collapse), unmount all rows
            // to ensure correct node-to-row mapping
            if (total_rows != last_total_rows_ && last_total_rows_ != -1)
            {
                for (auto& [idx, elem] : row_elements_)
                {
                    if (elem) elem->unmount();
                    rv.removeRowBox(idx);
                }
                row_elements_.clear();
            }

            int first_row = std::max(0, visible.first_row - row_buffer);
            int last_row = std::min(total_rows - 1, visible.last_row + row_buffer);

            // Unmount rows that scrolled out of view
            std::vector<int> to_remove;
            for (auto& [idx, elem] : row_elements_)
            {
                if (idx < first_row || idx > last_row)
                {
                    if (elem) elem->unmount();
                    rv.removeRowBox(idx);
                    to_remove.push_back(idx);
                }
            }
            for (auto idx : to_remove)
                row_elements_.erase(idx);

            // Mount rows newly in view
            for (int i = first_row; i <= last_row; ++i)
            {
                if (row_elements_.count(i))
                    continue; // Already mounted

                auto info = rv.getRowInfo(i);
                if (!info.node)
                    continue;

                WidgetRef row_widget = w.row_builder(
                    *this,
                    *info.node,
                    info.depth,
                    info.is_expanded,
                    info.has_children);

                if (!row_widget)
                    continue;

                auto elem = updateChild(nullptr, row_widget, this);
                if (elem)
                {
                    auto* roe = elem->findDescendantRenderObjectElement();
                    if (roe)
                    {
                        auto box = std::dynamic_pointer_cast<RenderBox>(
                            roe->sharedRenderObject());
                        if (box)
                            rv.setRowBox(i, std::move(box));
                    }
                    row_elements_[i] = std::move(elem);
                }
            }

            last_first_row_ = first_row;
            last_last_row_ = last_row;
            last_total_rows_ = total_rows;
        }

    private:
        RenderTreeView& renderTreeView() const
        {
            return static_cast<RenderTreeView&>(*render_object_);
        }

        void wireCallback()
        {
            renderTreeView().on_visible_range_changed = [this]()
            {
                markNeedsBuild();
                rebuild();
            };
        }

        void wireController()
        {
            const auto& w = static_cast<const TreeView&>(*widget_);

            // Remove old listener if any
            if (controller_listener_id_ != 0)
            {
                if (w.controller)
                    w.controller->removeListener(controller_listener_id_);
                controller_listener_id_ = 0;
            }

            // Add new listener
            if (w.controller)
            {
                controller_listener_id_ = w.controller->addListener([this]()
                {
                    // Expansion state changed - invalidate render object's cache
                    renderTreeView().invalidateRowCache();
                    markNeedsBuild();
                    rebuild();
                });
            }
        }

        std::unordered_map<int, std::shared_ptr<Element>> row_elements_;
        uint64_t controller_listener_id_ = 0;

        // Track last range to avoid unnecessary rebuilds
        int last_first_row_ = -1;
        int last_last_row_ = -1;
        int last_total_rows_ = -1;
    };

    // =========================================================================
    // TreeView widget
    // =========================================================================

    std::shared_ptr<Element> TreeView::createElement() const
    {
        return std::make_shared<TreeViewElement>(
            std::static_pointer_cast<const TreeView>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> TreeView::createRenderObject() const
    {
        auto render = std::make_shared<RenderTreeView>();
        render->root = root;
        render->controller = controller ? controller : std::make_shared<TreeController>();
        render->indent_width = indent_width;
        render->row_height = row_height;
        render->setHorizontalController(horizontal_controller);
        render->setVerticalController(vertical_controller);
        render->setPhysics(physics);
        return render;
    }

    void TreeView::updateRenderObject(RenderObject& render_object) const
    {
        auto& rv = static_cast<RenderTreeView&>(render_object);
        rv.root = root;
        // Only update controller if we have an external one, otherwise preserve internal
        if (controller)
            rv.controller = controller;
        rv.indent_width = indent_width;
        rv.row_height = row_height;
        rv.setHorizontalController(horizontal_controller);
        rv.setVerticalController(vertical_controller);
        rv.setPhysics(physics);
    }

} // namespace systems::leal::campello_widgets
