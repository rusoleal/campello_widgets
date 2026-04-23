#include <algorithm>
#include <cmath>
#include <limits>
#include <campello_widgets/ui/render_table_view.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderTableView::RenderTableView()
    {
        physics_ = std::make_shared<ClampingScrollPhysics>();
    }

    RenderTableView::~RenderTableView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (horizontal_controller_) horizontal_controller_->detach();
        if (vertical_controller_) vertical_controller_->detach();
    }

    void RenderTableView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderTableView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderTableView::setHorizontalController(std::shared_ptr<ScrollController> controller)
    {
        if (horizontal_controller_) horizontal_controller_->detach();
        horizontal_controller_ = std::move(controller);
        if (horizontal_controller_) horizontal_controller_->attach();
    }

    void RenderTableView::setVerticalController(std::shared_ptr<ScrollController> controller)
    {
        if (vertical_controller_) vertical_controller_->detach();
        vertical_controller_ = std::move(controller);
        if (vertical_controller_) vertical_controller_->attach();
    }

    void RenderTableView::setPhysics(std::shared_ptr<ScrollPhysics> physics)
    {
        physics_ = physics ? std::move(physics) : std::make_shared<ClampingScrollPhysics>();
    }

    void RenderTableView::setCell(int row, int column, std::shared_ptr<RenderBox> box)
    {
        uint64_t key = cellKey(row, column);
        auto it = cells_.find(key);
        if (it != cells_.end() && it->second.box)
            it->second.box->setParent(nullptr);

        if (box) box->setParent(this);
        cells_[key] = {std::move(box), Offset::zero()};
        markNeedsLayout();
    }

    void RenderTableView::removeCell(int row, int column)
    {
        uint64_t key = cellKey(row, column);
        auto it = cells_.find(key);
        if (it != cells_.end())
        {
            if (it->second.box) it->second.box->setParent(nullptr);
            cells_.erase(it);
        }
        markNeedsLayout();
    }

    RenderTableView::VisibleRange RenderTableView::visibleRange() const noexcept
    {
        if (extents.row_count <= 0 || extents.column_count <= 0)
            return {0, -1, 0, -1};

        const int pinned_rows = computePinnedRowCount();
        const int pinned_cols = computePinnedColumnCount();
        const float scroll_x = scrollX();
        const float scroll_y = scrollY();

        // Find first visible row (including pinned rows)
        int first_row = pinned_rows; // Start after pinned rows
        float y = computeRowOffset(first_row);
        for (int r = first_row; r < extents.row_count; ++r)
        {
            float row_height = (r < static_cast<int>(row_spans.size())) ? row_spans[r].extent : 0.0f;
            if (y + row_height > scroll_y)
            {
                first_row = r;
                break;
            }
            y += row_height + ((r < static_cast<int>(row_spans.size())) ? row_spans[r].padding : 0.0f);
        }

        // Find last visible row
        int last_row = std::max(first_row, pinned_rows);
        y = computeRowOffset(last_row);
        for (int r = last_row; r < extents.row_count && y < scroll_y + viewport_height_; ++r)
        {
            float row_height = (r < static_cast<int>(row_spans.size())) ? row_spans[r].extent : 0.0f;
            y += row_height;
            last_row = r;
            y += (r < static_cast<int>(row_spans.size())) ? row_spans[r].padding : 0.0f;
        }

        // Find first visible column (including pinned columns)
        int first_col = pinned_cols; // Start after pinned columns
        float x = computeColumnOffset(first_col);
        for (int c = first_col; c < extents.column_count; ++c)
        {
            float col_width = (c < static_cast<int>(column_spans.size())) ? column_spans[c].extent : 0.0f;
            if (x + col_width > scroll_x)
            {
                first_col = c;
                break;
            }
            x += col_width + ((c < static_cast<int>(column_spans.size())) ? column_spans[c].padding : 0.0f);
        }

        // Find last visible column
        int last_col = std::max(first_col, pinned_cols);
        x = computeColumnOffset(last_col);
        for (int c = last_col; c < extents.column_count && x < scroll_x + viewport_width_; ++c)
        {
            float col_width = (c < static_cast<int>(column_spans.size())) ? column_spans[c].extent : 0.0f;
            x += col_width;
            last_col = c;
            x += (c < static_cast<int>(column_spans.size())) ? column_spans[c].padding : 0.0f;
        }

        // Include pinned rows/columns in the visible range
        first_row = 0;
        first_col = 0;

        return {first_row, last_row, first_col, last_col};
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderTableView::performLayout()
    {
        // Table fills available space
        size_ = constraints_.constrain({
            constraints_.max_width,
            constraints_.max_height,
        });

        viewport_width_ = size_.width;
        viewport_height_ = size_.height;

        // Compute scroll extents
        float total_width = computeTotalWidth();
        float total_height = computeTotalHeight();

        min_scroll_x_ = 0.0f;
        max_scroll_x_ = std::max(0.0f, total_width - viewport_width_);
        min_scroll_y_ = 0.0f;
        max_scroll_y_ = std::max(0.0f, total_height - viewport_height_);

        // Update controllers
        if (horizontal_controller_)
            horizontal_controller_->setExtents(min_scroll_x_, max_scroll_x_);
        else
            internal_scroll_x_ = std::clamp(internal_scroll_x_, min_scroll_x_, max_scroll_x_);

        if (vertical_controller_)
            vertical_controller_->setExtents(min_scroll_y_, max_scroll_y_);
        else
            internal_scroll_y_ = std::clamp(internal_scroll_y_, min_scroll_y_, max_scroll_y_);

        // Layout visible cells
        const float scroll_x = scrollX();
        const float scroll_y = scrollY();
        const int pinned_rows = computePinnedRowCount();
        const int pinned_cols = computePinnedColumnCount();
        const float pinned_rows_height = computePinnedRowsHeight();
        const float pinned_cols_width = computePinnedColumnsWidth();
        (void)pinned_rows_height; // Used in clip calculations
        (void)pinned_cols_width;  // Used in clip calculations

        for (auto& [key, entry] : cells_)
        {
            if (!entry.box) continue;

            int row, col;
            decodeCellKey(key, row, col);

            if (row < 0 || row >= extents.row_count || col < 0 || col >= extents.column_count)
                continue;

            float row_offset = computeRowOffset(row);
            float col_offset = computeColumnOffset(col);

            float row_height = (row < static_cast<int>(row_spans.size())) ? row_spans[row].extent : 0.0f;
            float col_width = (col < static_cast<int>(column_spans.size())) ? column_spans[col].extent : 0.0f;

            // Apply scroll offset for non-pinned cells
            bool is_pinned_row = row < pinned_rows;
            bool is_pinned_col = col < pinned_cols;

            // Pinned cells stay at their original position
            // Non-pinned cells scroll with the viewport
            float cell_x = is_pinned_col ? col_offset : col_offset - scroll_x;
            float cell_y = is_pinned_row ? row_offset : row_offset - scroll_y;

            BoxConstraints cell_constraints = BoxConstraints::tight(col_width, row_height);
            entry.box->layout(cell_constraints);
            entry.offset = Offset{cell_x, cell_y};
        }

        checkVisibleRangeChanged();
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderTableView::performPaint(PaintContext& context, const Offset& offset)
    {
        if (cells_.empty()) return;

        auto& canvas = context.canvas();
        const int pinned_rows = computePinnedRowCount();
        const int pinned_cols = computePinnedColumnCount();
        const float pinned_rows_height = computePinnedRowsHeight();
        const float pinned_cols_width = computePinnedColumnsWidth();

        // Define clip regions
        Rect viewport = Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height);
        Rect pinned_row_clip = Rect::fromLTWH(
            offset.x + pinned_cols_width, offset.y,
            size_.width - pinned_cols_width, pinned_rows_height);
        Rect pinned_col_clip = Rect::fromLTWH(
            offset.x, offset.y + pinned_rows_height,
            pinned_cols_width, size_.height - pinned_rows_height);
        Rect scrollable_clip = Rect::fromLTWH(
            offset.x + pinned_cols_width, offset.y + pinned_rows_height,
            size_.width - pinned_cols_width, size_.height - pinned_rows_height);
        Rect corner_clip = Rect::fromLTWH(
            offset.x, offset.y, pinned_cols_width, pinned_rows_height);

        // Collect and sort cells by paint order (z-index)
        // Order: regular (z=0) < pinned cols (z=1) < pinned rows (z=2) < corner (z=3)
        struct CellToPaint
        {
            const CellEntry* entry;
            int row;
            int col;
            int z_order;
            Rect clip;
        };
        std::vector<CellToPaint> cells_to_paint;
        cells_to_paint.reserve(cells_.size());

        for (const auto& [key, entry] : cells_)
        {
            if (!entry.box) continue;

            int row, col;
            decodeCellKey(key, row, col);

            // Skip cells outside viewport bounds
            float cell_right = entry.offset.x + entry.box->size().width;
            float cell_bottom = entry.offset.y + entry.box->size().height;
            if (cell_right < 0 || cell_bottom < 0 ||
                entry.offset.x > size_.width || entry.offset.y > size_.height)
                continue;

            bool is_pinned_row = row < pinned_rows;
            bool is_pinned_col = col < pinned_cols;

            int z_order;
            Rect clip;
            if (is_pinned_row && is_pinned_col)
            {
                z_order = 3; // Corner on top
                clip = corner_clip;
            }
            else if (is_pinned_row)
            {
                z_order = 2; // Pinned row
                clip = pinned_row_clip;
            }
            else if (is_pinned_col)
            {
                z_order = 1; // Pinned column
                clip = pinned_col_clip;
            }
            else
            {
                z_order = 0; // Regular cell
                clip = scrollable_clip;
            }

            cells_to_paint.push_back({&entry, row, col, z_order, clip});
        }

        // Sort by z_order
        std::sort(cells_to_paint.begin(), cells_to_paint.end(),
            [](const CellToPaint& a, const CellToPaint& b) {
                return a.z_order < b.z_order;
            });

        // Clip to viewport first
        canvas.save();
        canvas.clipRect(viewport);

        // Paint cells with per-cell clipping
        int current_z = -1;
        for (const auto& cell : cells_to_paint)
        {
            // When z-order changes, restore and re-clip for new layer
            if (cell.z_order != current_z)
            {
                if (current_z >= 0) canvas.restore();
                canvas.save();
                canvas.clipRect(cell.clip);
                current_z = cell.z_order;
            }

            // Get cell size for clipping
            Size cell_size = cell.entry->box->size();
            float cell_w = cell_size.width;
            float cell_h = cell_size.height;
            Rect cell_clip = Rect::fromLTWH(
                offset.x + cell.entry->offset.x,
                offset.y + cell.entry->offset.y,
                cell_w, cell_h);

            // Clip to cell bounds and paint
            canvas.save();
            canvas.clipRect(cell_clip);
            cell.entry->box->paint(context, offset + cell.entry->offset);
            canvas.restore();
        }

        // Restore all clip states
        if (current_z >= 0) canvas.restore();
        canvas.restore();
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    bool RenderTableView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        // Hit test in reverse paint order (pinned corner first)
        const int pinned_rows = computePinnedRowCount();
        const int pinned_cols = computePinnedColumnCount();

        // Collect cells by category
        std::vector<const CellEntry*> pinned_corner;
        std::vector<const CellEntry*> pinned_rows_only;
        std::vector<const CellEntry*> pinned_cols_only;
        std::vector<const CellEntry*> regular;

        for (const auto& [key, entry] : cells_)
        {
            if (!entry.box) continue;

            int row, col;
            decodeCellKey(key, row, col);

            bool is_pinned_row = row < pinned_rows;
            bool is_pinned_col = col < pinned_cols;

            if (is_pinned_row && is_pinned_col)
                pinned_corner.push_back(&entry);
            else if (is_pinned_row)
                pinned_rows_only.push_back(&entry);
            else if (is_pinned_col)
                pinned_cols_only.push_back(&entry);
            else
                regular.push_back(&entry);
        }

        // Hit test in reverse order: pinned corner, pinned cols, pinned rows, regular
        auto testList = [&](const std::vector<const CellEntry*>& list) -> bool
        {
            for (auto it = list.rbegin(); it != list.rend(); ++it)
            {
                const auto& entry = **it;
                // Adjust position for cell's local coordinates
                Offset local_pos = position - entry.offset;
                if (entry.box->hitTest(result, local_pos))
                    return true;
            }
            return false;
        };

        if (testList(pinned_corner)) return true;
        if (testList(pinned_cols_only)) return true;
        if (testList(pinned_rows_only)) return true;
        if (testList(regular)) return true;

        return false;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    float RenderTableView::scrollX() const noexcept
    {
        return horizontal_controller_ ? horizontal_controller_->offset() : internal_scroll_x_;
    }

    float RenderTableView::scrollY() const noexcept
    {
        return vertical_controller_ ? vertical_controller_->offset() : internal_scroll_y_;
    }

    void RenderTableView::applyScrollDelta(float dx, float dy)
    {
        // Apply horizontal scroll
        if (std::abs(dx) > 0.0f)
        {
            const float raw = scrollX() + dx;
            const float clamped = physics_->applyBoundaryConditions(raw, min_scroll_x_, max_scroll_x_);

            if (horizontal_controller_)
                horizontal_controller_->jumpTo(clamped);
            else
                internal_scroll_x_ = clamped;
        }

        // Apply vertical scroll
        if (std::abs(dy) > 0.0f)
        {
            const float raw = scrollY() + dy;
            const float clamped = physics_->applyBoundaryConditions(raw, min_scroll_y_, max_scroll_y_);

            if (vertical_controller_)
                vertical_controller_->jumpTo(clamped);
            else
                internal_scroll_y_ = clamped;
        }

        markNeedsLayout();
        checkVisibleRangeChanged();
    }

    void RenderTableView::checkVisibleRangeChanged()
    {
        auto current = visibleRange();
        if (current.first_row != cached_range_.first_row ||
            current.last_row != cached_range_.last_row ||
            current.first_col != cached_range_.first_col ||
            current.last_col != cached_range_.last_col)
        {
            cached_range_ = current;
            if (on_visible_range_changed) on_visible_range_changed();
        }
    }

    float RenderTableView::computeRowOffset(int row) const
    {
        float offset = 0.0f;
        for (int r = 0; r < row && r < extents.row_count; ++r)
        {
            if (r < static_cast<int>(row_spans.size()))
            {
                offset += row_spans[r].extent + row_spans[r].padding;
            }
        }
        return offset;
    }

    float RenderTableView::computeColumnOffset(int col) const
    {
        float offset = 0.0f;
        for (int c = 0; c < col && c < extents.column_count; ++c)
        {
            if (c < static_cast<int>(column_spans.size()))
            {
                offset += column_spans[c].extent + column_spans[c].padding;
            }
        }
        return offset;
    }

    float RenderTableView::computeTotalHeight() const
    {
        float height = 0.0f;
        for (int r = 0; r < extents.row_count && r < static_cast<int>(row_spans.size()); ++r)
        {
            height += row_spans[r].extent + row_spans[r].padding;
        }
        return height;
    }

    float RenderTableView::computeTotalWidth() const
    {
        float width = 0.0f;
        for (int c = 0; c < extents.column_count && c < static_cast<int>(column_spans.size()); ++c)
        {
            width += column_spans[c].extent + column_spans[c].padding;
        }
        return width;
    }

    int RenderTableView::computePinnedRowCount() const
    {
        int count = 0;
        for (const auto& span : row_spans)
        {
            if (span.pinned) ++count;
            else break;
        }
        return count;
    }

    int RenderTableView::computePinnedColumnCount() const
    {
        int count = 0;
        for (const auto& span : column_spans)
        {
            if (span.pinned) ++count;
            else break;
        }
        return count;
    }

    float RenderTableView::computePinnedRowsHeight() const
    {
        float height = 0.0f;
        for (const auto& span : row_spans)
        {
            if (span.pinned) height += span.extent + span.padding;
            else break;
        }
        return height;
    }

    float RenderTableView::computePinnedColumnsWidth() const
    {
        float width = 0.0f;
        for (const auto& span : column_spans)
        {
            if (span.pinned) width += span.extent + span.padding;
            else break;
        }
        return width;
    }

    uint64_t RenderTableView::cellKey(int row, int col)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(row)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(col));
    }

    void RenderTableView::decodeCellKey(uint64_t key, int& row, int& col)
    {
        row = static_cast<int>(static_cast<uint32_t>(key >> 32));
        col = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
    }

    // -------------------------------------------------------------------------
    // Input handling
    // -------------------------------------------------------------------------

    void RenderTableView::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pointer_down_ = true;
            panning_ = false;
            pan_last_pos_ = event.position;
            velocity_x_ = 0.0f;
            velocity_y_ = 0.0f;
            pan_velocity_x_ = 0.0f;
            pan_velocity_y_ = 0.0f;
            last_pan_time_ = std::chrono::steady_clock::now();
            break;

        case PointerEventKind::move:
        {
            // Pan gesture disabled - only scroll wheel scrolling allowed
            // Track position for tap detection but don't scroll
            pan_last_pos_ = event.position;
            break;
        }

        case PointerEventKind::up:
            pointer_down_ = false;
            if (panning_ && physics_->allowsMomentum())
            {
                velocity_x_ = pan_velocity_x_;
                velocity_y_ = pan_velocity_y_;
            }
            panning_ = false;
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            break;

        case PointerEventKind::scroll:
            // Invert scroll direction for natural scrolling feel on macOS
            applyScrollDelta(-event.scroll_delta_x, -event.scroll_delta_y);
            break;
        }
    }

    void RenderTableView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        float scroll_x = scrollX();
        float scroll_y = scrollY();

        // Handle overscroll springback for X
        if (scroll_x < min_scroll_x_ || scroll_x > max_scroll_x_)
        {
            float target_x = std::clamp(scroll_x, min_scroll_x_, max_scroll_x_);
            float spring_vx = (target_x - scroll_x) * kSpringCoeff;
            velocity_x_ = spring_vx + velocity_x_ * 0.7f;
        }
        else if (std::abs(velocity_x_) >= kMinVelocity)
        {
            velocity_x_ = physics_->applyFriction(velocity_x_, dt_s);
        }
        else
        {
            velocity_x_ = 0.0f;
        }

        // Handle overscroll springback for Y
        if (scroll_y < min_scroll_y_ || scroll_y > max_scroll_y_)
        {
            float target_y = std::clamp(scroll_y, min_scroll_y_, max_scroll_y_);
            float spring_vy = (target_y - scroll_y) * kSpringCoeff;
            velocity_y_ = spring_vy + velocity_y_ * 0.7f;
        }
        else if (std::abs(velocity_y_) >= kMinVelocity)
        {
            velocity_y_ = physics_->applyFriction(velocity_y_, dt_s);
        }
        else
        {
            velocity_y_ = 0.0f;
        }

        if (std::abs(velocity_x_) > 0.0f || std::abs(velocity_y_) > 0.0f)
        {
            applyScrollDelta(velocity_x_ * dt_s, velocity_y_ * dt_s);
        }
    }


    void RenderTableView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : cells_)
            if (c.second.box.get()) visitor(c.second.box.get());
    }
} // namespace systems::leal::campello_widgets
