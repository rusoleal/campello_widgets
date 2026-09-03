#include <algorithm>
#include <cmath>
#include <limits>
#include <campello_widgets/ui/render_tree_view.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderTreeView::RenderTreeView()
    {
        physics_ = std::make_shared<ClampingScrollPhysics>();
    }

    RenderTreeView::~RenderTreeView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (horizontal_controller_) horizontal_controller_->detach();
        if (vertical_controller_) vertical_controller_->detach();
    }

    void RenderTreeView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderTreeView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderTreeView::setHorizontalController(std::shared_ptr<ScrollController> controller)
    {
        if (horizontal_controller_) horizontal_controller_->detach();
        horizontal_controller_ = std::move(controller);
        if (horizontal_controller_) horizontal_controller_->attach();
    }

    void RenderTreeView::setVerticalController(std::shared_ptr<ScrollController> controller)
    {
        if (vertical_controller_) vertical_controller_->detach();
        vertical_controller_ = std::move(controller);
        if (vertical_controller_) vertical_controller_->attach();
    }

    void RenderTreeView::setPhysics(std::shared_ptr<ScrollPhysics> physics)
    {
        physics_ = physics ? std::move(physics) : std::make_shared<ClampingScrollPhysics>();
    }

    void RenderTreeView::setRowBox(int row_index, std::shared_ptr<RenderBox> box)
    {
        auto it = rows_.find(row_index);
        if (it != rows_.end() && it->second.box)
            it->second.box->setParent(nullptr);

        if (box) box->setParent(this);
        rows_[row_index] = {std::move(box), Offset::zero()};
        markNeedsLayout();
    }

    void RenderTreeView::removeRowBox(int row_index)
    {
        auto it = rows_.find(row_index);
        if (it != rows_.end())
        {
            if (it->second.box) it->second.box->setParent(nullptr);
            rows_.erase(it);
        }
        markNeedsLayout();
    }

    RenderTreeView::VisibleRange RenderTreeView::visibleRange() const noexcept
    {
        if (row_height <= 0.0f)
            return {0, -1};

        int total_rows = computeTotalRows();
        if (total_rows <= 0)
            return {0, -1};

        float scroll_y = scrollY();
        int first_row = static_cast<int>(scroll_y / row_height);
        first_row = std::max(0, std::min(first_row, total_rows - 1));

        int visible_count = static_cast<int>(std::ceil(viewport_height_ / row_height)) + 1;
        int last_row = std::min(first_row + visible_count, total_rows - 1);

        return {first_row, last_row};
    }

    RenderTreeView::RowInfo RenderTreeView::getRowInfo(int row_index) const
    {
        if (row_cache_dirty_)
            rebuildRowCache();

        if (row_index < 0 || row_index >= static_cast<int>(row_cache_.size()))
            return RowInfo{};

        return row_cache_[row_index];
    }

    int RenderTreeView::computeTotalRows() const
    {
        if (row_cache_dirty_)
            rebuildRowCache();
        return cached_total_rows_;
    }

    void RenderTreeView::invalidateRowCache()
    {
        row_cache_dirty_ = true;
        markNeedsLayout();
    }

    void RenderTreeView::rebuildRowCache() const
    {
        row_cache_.clear();

        if (!root)
        {
            cached_total_rows_ = 0;
            row_cache_dirty_ = false;
            return;
        }

        // Build flattened row list using depth-first traversal
        fillRowCache(root.get(), 0);

        cached_total_rows_ = static_cast<int>(row_cache_.size());
        row_cache_dirty_ = false;
    }

    int RenderTreeView::countVisibleRows(const TreeNode* node, int depth) const
    {
        if (!node) return 0;

        int count = 1; // This node

        if (controller && controller->isExpanded(node))
        {
            for (const auto& child : node->children)
            {
                count += countVisibleRows(child.get(), depth + 1);
            }
        }

        return count;
    }

    void RenderTreeView::fillRowCache(const TreeNode* node, int depth) const
    {
        if (!node) return;

        bool has_children = node->hasChildren();
        bool is_expanded = controller ? controller->isExpanded(node) : false;

        row_cache_.push_back(RowInfo{
            node,
            depth,
            has_children,
            is_expanded
        });

        if (is_expanded)
        {
            for (const auto& child : node->children)
            {
                fillRowCache(child.get(), depth + 1);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderTreeView::performLayout()
    {
        // Fill available space
        size_ = constraints_.constrain({
            constraints_.max_width,
            constraints_.max_height,
        });

        viewport_width_ = size_.width;
        viewport_height_ = size_.height;

        // Invalidate row cache if tree structure might have changed
        row_cache_dirty_ = true;

        // Compute scroll extents
        int total_rows = computeTotalRows();
        float total_height = total_rows * row_height;
        float max_depth = computeMaxDepth();
        float total_width = max_depth * indent_width + 200.0f; // Add padding for content

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

        // Layout visible rows
        float scroll_x = scrollX();
        float scroll_y = scrollY();

        for (auto& [idx, entry] : rows_)
        {
            if (!entry.box) continue;

            RowInfo info = getRowInfo(idx);
            if (!info.node) continue;

            float row_y = idx * row_height - scroll_y;
            float indent_x = info.depth * indent_width - scroll_x;

            // Row fills viewport width
            BoxConstraints row_constraints = BoxConstraints::tight(viewport_width_, row_height);
            entry.box->layout(row_constraints);
            entry.offset = Offset{indent_x, row_y};
        }

        checkVisibleRangeChanged();
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderTreeView::performPaint(PaintContext& context, const Offset& offset)
    {
        if (rows_.empty()) return;

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));

        for (const auto& [idx, entry] : rows_)
        {
            if (entry.box)
                entry.box->paint(context, offset + entry.offset);
        }

        canvas.restore();
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    bool RenderTreeView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        // Collect entries in a list for reverse iteration
        std::vector<const RowEntry*> entries;
        entries.reserve(rows_.size());
        for (const auto& [idx, entry] : rows_)
            entries.push_back(&entry);

        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (!(*it)->box) continue;
            Offset local_pos = position - (*it)->offset;
            if ((*it)->box->hitTest(result, local_pos))
                return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    float RenderTreeView::scrollX() const noexcept
    {
        return horizontal_controller_ ? horizontal_controller_->offset() : internal_scroll_x_;
    }

    float RenderTreeView::scrollY() const noexcept
    {
        return vertical_controller_ ? vertical_controller_->offset() : internal_scroll_y_;
    }

    void RenderTreeView::applyScrollDelta(float dx, float dy)
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

    void RenderTreeView::checkVisibleRangeChanged()
    {
        auto current = visibleRange();
        if (current.first_row != cached_range_.first_row ||
            current.last_row != cached_range_.last_row)
        {
            cached_range_ = current;
            if (on_visible_range_changed) on_visible_range_changed();
        }
    }

    float RenderTreeView::computeTotalWidth() const
    {
        return computeMaxDepth() * indent_width + 200.0f;
    }

    float RenderTreeView::computeMaxDepth() const
    {
        if (!root) return 0;

        float max_depth = 0;
        std::vector<std::pair<const TreeNode*, int>> stack;
        stack.push_back({root.get(), 0});

        while (!stack.empty())
        {
            auto [node, depth] = stack.back();
            stack.pop_back();

            max_depth = std::max(max_depth, static_cast<float>(depth));

            if (controller && controller->isExpanded(node))
            {
                for (const auto& child : node->children)
                {
                    if (child)
                        stack.push_back({child.get(), depth + 1});
                }
            }
        }

        return max_depth;
    }

    // -------------------------------------------------------------------------
    // Input handling
    // -------------------------------------------------------------------------

    void RenderTreeView::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pointer_down_ = true;
            panning_ = false;
            won_arena_ = false;
            lost_arena_ = false;
            pan_last_pos_ = event.position;
            pan_down_pos_ = event.position;
            device_kind_ = event.device_kind;
            velocity_x_ = 0.0f;
            velocity_y_ = 0.0f;
            velocity_tracker_x_.reset();
            velocity_tracker_y_.reset();
            velocity_tracker_x_.addPosition(event.timestamp_ms, event.position.x);
            velocity_tracker_y_.addPosition(event.timestamp_ms, event.position.y);
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            break;

        case PointerEventKind::move:
        {
            // Only process move events for scrolling if pointer is down
            if (!pointer_down_ || lost_arena_)
                break;

            float dx = event.position.x - pan_last_pos_.x;
            float dy = event.position.y - pan_last_pos_.y;

            // The slop check measures cumulative distance from pointer-down
            // (pan_down_pos_, fixed for the whole gesture), NOT the
            // frame-to-frame delta (dx/dy above) — touch delivery arrives in
            // many small increments, so checking each one individually
            // against the slop threshold means a slow-building drag whose
            // per-frame steps never individually exceed it would never start
            // panning at all, no matter how far the finger travels in total.
            float total_dx = event.position.x - pan_down_pos_.x;
            float total_dy = event.position.y - pan_down_pos_.y;
            float distance = std::sqrt(total_dx * total_dx + total_dy * total_dy);

            if (!panning_ && distance > computePanSlop(device_kind_))
            {
                if (won_arena_)
                {
                    panning_ = true;
                }
                else if (arena_entry_)
                {
                    // We have no competitor left to lose to once the arena is
                    // closed (an actual competitor would already have set
                    // lost_arena_ via rejectGesture() before this point), so
                    // this always resolves synchronously in our favor.
                    arena_entry_->resolve(GestureDisposition::accepted);
                    panning_ = true;
                }
            }

            if (panning_)
            {
                applyScrollDelta(-dx, -dy);

                velocity_tracker_x_.addPosition(event.timestamp_ms, event.position.x);
                velocity_tracker_y_.addPosition(event.timestamp_ms, event.position.y);
            }

            pan_last_pos_ = event.position;
            break;
        }

        case PointerEventKind::up:
            pointer_down_ = false;
            if (panning_ && physics_->allowsMomentum())
            {
                velocity_x_ = -velocity_tracker_x_.getVelocity();
                velocity_y_ = -velocity_tracker_y_.getVelocity();
            }
            panning_ = false;
            arena_entry_.reset();
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            arena_entry_.reset();
            break;

        case PointerEventKind::scroll:
        {
            applyScrollDelta(event.scroll_delta_x, event.scroll_delta_y);
            // See RenderListView::applyScrollDelta()'s doc on wheel_velocity_tracker_.
            wheel_velocity_tracker_x_.addPosition(event.timestamp_ms, scrollX());
            wheel_velocity_tracker_y_.addPosition(event.timestamp_ms, scrollY());
            wheel_momentum_pending_ = true;
            // Out of scope for the VelocityTracker timestamp-injection fix
            // (see gesture_constants.hpp's currentMonotonicMs() doc / the
            // plan this change came from) -- kept on its own independent
            // steady_clock::now() read, same as before.
            last_scroll_event_ms_ = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            break;
        }
        }
    }

    // -------------------------------------------------------------------------
    // GestureArenaMember
    // -------------------------------------------------------------------------

    void RenderTreeView::acceptGesture(int32_t /*pointer_id*/)
    {
        // Only records the win. panning_ starts once a move actually exceeds
        // pan slop (see onPointerEvent) — accepting here just means we're no
        // longer competing, not that movement has happened yet (e.g. a sole,
        // uncontested arena resolves immediately at pointer-down).
        won_arena_ = true;
    }

    void RenderTreeView::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        panning_    = false;
    }

    void RenderTreeView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        if (now_ms - last_scroll_event_ms_ < kScrollActiveWindowMs)
        {
            last_tick_ms_ = now_ms;
            // Frames are demand-driven on this platform — a frame is only
            // built when something requests one. Without self-requesting
            // the next frame here, spring-back/momentum only ever applies
            // once (whatever frame was already in flight) and then freezes
            // instead of animating. See RenderListView::onTick()'s doc for
            // the full explanation (must call FrameScheduler::scheduleFrame()
            // directly, not markNeedsPaint(), which no-ops mid-frame).
            const bool overscrolled = scrollX() < min_scroll_x_ || scrollX() > max_scroll_x_ ||
                                       scrollY() < min_scroll_y_ || scrollY() > max_scroll_y_;
            if (overscrolled || std::abs(velocity_x_) >= kMinVelocity || std::abs(velocity_y_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            return;
        }

        // See RenderListView::onTick()'s doc on wheel_momentum_pending_.
        if (wheel_momentum_pending_)
        {
            wheel_momentum_pending_ = false;
            if (physics_->allowsMomentum())
            {
                velocity_x_ = wheel_velocity_tracker_x_.getVelocity();
                velocity_y_ = wheel_velocity_tracker_y_.getVelocity();
            }
        }

        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        float scroll_x = scrollX();
        float scroll_y = scrollY();
        bool  needs_update = false;
        float delta_x = 0.0f;
        float delta_y = 0.0f;

        // Handle overscroll springback for X. Exponential ease of the
        // overscroll distance toward zero — unconditionally stable
        // regardless of dt/frame rate, unlike a velocity-based spring
        // (`velocity = k*displacement + velocity*damping`), which can
        // accumulate velocity faster than the position converges and
        // overshoot past the target with real residual speed — carrying
        // that speed into the momentum phase below and bouncing off the
        // *opposite* edge, felt as sustained vibration rather than a
        // settle.
        if (scroll_x < min_scroll_x_ || scroll_x > max_scroll_x_)
        {
            float target_x = std::clamp(scroll_x, min_scroll_x_, max_scroll_x_);
            float eased_x  = (scroll_x - target_x) * std::exp(-kSpringCoeff * dt_s);
            delta_x        = (target_x + eased_x) - scroll_x;
            velocity_x_    = 0.0f;
            needs_update   = true;
        }
        else if (std::abs(velocity_x_) >= kMinVelocity)
        {
            velocity_x_  = physics_->applyFriction(velocity_x_, dt_s);
            delta_x      = velocity_x_ * dt_s;
            needs_update = true;
        }
        else
        {
            velocity_x_ = 0.0f;
        }

        // Handle overscroll springback for Y — same reasoning as X above.
        if (scroll_y < min_scroll_y_ || scroll_y > max_scroll_y_)
        {
            float target_y = std::clamp(scroll_y, min_scroll_y_, max_scroll_y_);
            float eased_y  = (scroll_y - target_y) * std::exp(-kSpringCoeff * dt_s);
            delta_y        = (target_y + eased_y) - scroll_y;
            velocity_y_    = 0.0f;
            needs_update   = true;
        }
        else if (std::abs(velocity_y_) >= kMinVelocity)
        {
            velocity_y_  = physics_->applyFriction(velocity_y_, dt_s);
            delta_y      = velocity_y_ * dt_s;
            needs_update = true;
        }
        else
        {
            velocity_y_ = 0.0f;
        }

        if (needs_update)
        {
            applyScrollDelta(delta_x, delta_y);
            // See the doc above on the trackpad-active branch — nothing
            // else asks for a follow-up frame once this one finishes
            // painting, so schedule the next tick ourselves whenever
            // there's still motion left to animate.
            if (std::abs(velocity_x_) >= kMinVelocity || std::abs(velocity_y_) >= kMinVelocity ||
                scrollX() < min_scroll_x_ || scrollX() > max_scroll_x_ ||
                scrollY() < min_scroll_y_ || scrollY() > max_scroll_y_)
                FrameScheduler::scheduleFrame();
        }
    }


    void RenderTreeView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : rows_)
            if (c.second.box.get()) visitor(c.second.box.get());
    }
} // namespace systems::leal::campello_widgets
