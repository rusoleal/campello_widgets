#include <algorithm>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/table_view.hpp>
#include <campello_widgets/ui/render_table_view.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // TableViewElement - builds only cells in the current visible range
    // =========================================================================

    class TableViewElement : public RenderObjectElement
    {
    public:
        explicit TableViewElement(std::shared_ptr<const TableView> widget)
            : RenderObjectElement(std::move(widget))
        {}

        void mount(Element* parent) override
        {
            RenderObjectElement::mount(parent);
            wireCallback();
        }

        void unmount() override
        {
            // Unmount all cell elements
            for (auto& [key, elem] : cell_elements_)
            {
                if (elem) elem->unmount();
            }
            cell_elements_.clear();

            renderTableView().on_visible_range_changed = nullptr;
            RenderObjectElement::unmount();
        }

        void update(WidgetRef new_widget) override
        {
            RenderObjectElement::update(std::move(new_widget));
            wireCallback();
            rebuild();
        }

    protected:
        void performBuild() override
        {
            const auto& w = static_cast<const TableView&>(*widget_);
            auto& rv = renderTableView();

            if (!w.cell_builder || w.extents.row_count <= 0 || w.extents.column_count <= 0)
                return;

            // Get current visible range with buffer
            auto visible = rv.visibleRange();
            if (visible.last_row < visible.first_row || visible.last_col < visible.first_col)
                return;

            // Add buffer around visible area
            const int row_buffer = 1;
            const int col_buffer = 1;

            int first_row = std::max(0, visible.first_row - row_buffer);
            int last_row = std::min(w.extents.row_count - 1, visible.last_row + row_buffer);
            int first_col = std::max(0, visible.first_col - col_buffer);
            int last_col = std::min(w.extents.column_count - 1, visible.last_col + col_buffer);

            // Unmount cells that scrolled out of view
            std::vector<uint64_t> to_remove;
            for (auto& [key, elem] : cell_elements_)
            {
                int row, col;
                decodeCellKey(key, row, col);

                if (row < first_row || row > last_row || col < first_col || col > last_col)
                {
                    if (elem) elem->unmount();
                    rv.removeCell(row, col);
                    to_remove.push_back(key);
                }
            }
            for (auto key : to_remove)
                cell_elements_.erase(key);

            // Mount cells newly in view
            for (int r = first_row; r <= last_row; ++r)
            {
                for (int c = first_col; c <= last_col; ++c)
                {
                    uint64_t key = cellKey(r, c);
                    if (cell_elements_.count(key))
                        continue; // Already mounted

                    WidgetRef cell_widget = w.cell_builder(*this, r, c);
                    if (!cell_widget)
                        continue;

                    auto elem = updateChild(nullptr, cell_widget, this);
                    if (elem)
                    {
                        auto* roe = elem->findDescendantRenderObjectElement();
                        if (roe)
                        {
                            auto box = std::dynamic_pointer_cast<RenderBox>(
                                roe->sharedRenderObject());
                            if (box)
                                rv.setCell(r, c, std::move(box));
                        }
                        cell_elements_[key] = std::move(elem);
                    }
                }
            }

            // Update cached range
            last_range_ = {first_row, last_row, first_col, last_col};
        }

    private:
        RenderTableView& renderTableView() const
        {
            return static_cast<RenderTableView&>(*render_object_);
        }

        void wireCallback()
        {
            renderTableView().on_visible_range_changed = [this]()
            {
                markNeedsBuild();
                rebuild();
            };
        }

        // Cell key encoding: (row << 32) | col
        static uint64_t cellKey(int row, int col)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(row)) << 32) |
                   static_cast<uint64_t>(static_cast<uint32_t>(col));
        }

        static void decodeCellKey(uint64_t key, int& row, int& col)
        {
            row = static_cast<int>(static_cast<uint32_t>(key >> 32));
            col = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
        }

        std::unordered_map<uint64_t, std::shared_ptr<Element>> cell_elements_;

        // Track current visible range to avoid unnecessary rebuilds
        struct Range
        {
            int first_row = -1, last_row = -1;
            int first_col = -1, last_col = -1;
        };
        Range last_range_;
    };

    // =========================================================================
    // TableView widget
    // =========================================================================

    std::shared_ptr<Element> TableView::createElement() const
    {
        return std::make_shared<TableViewElement>(
            std::static_pointer_cast<const TableView>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> TableView::createRenderObject() const
    {
        auto render = std::make_shared<RenderTableView>();
        render->extents = extents;
        render->row_spans = row_spans;
        render->column_spans = column_spans;
        render->setHorizontalController(horizontal_controller);
        render->setVerticalController(vertical_controller);
        render->setPhysics(physics);
        return render;
    }

    void TableView::updateRenderObject(RenderObject& render_object) const
    {
        auto& rv = static_cast<RenderTableView&>(render_object);
        rv.extents = extents;
        rv.row_spans = row_spans;
        rv.column_spans = column_spans;
        rv.setHorizontalController(horizontal_controller);
        rv.setVerticalController(vertical_controller);
        rv.setPhysics(physics);
    }

} // namespace systems::leal::campello_widgets
