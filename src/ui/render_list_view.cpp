#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderListView::RenderListView()
    {
        physics_ = std::make_shared<ClampingScrollPhysics>();
    }

    RenderListView::~RenderListView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (controller_) controller_->detach();
    }

    void RenderListView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderListView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderListView::setController(std::shared_ptr<ScrollController> controller)
    {
        if (controller_) controller_->detach();
        controller_ = std::move(controller);
        if (controller_) controller_->attach();
    }

    void RenderListView::setPhysics(std::shared_ptr<ScrollPhysics> physics)
    {
        physics_ = physics ? std::move(physics) : std::make_shared<ClampingScrollPhysics>();
    }

    void RenderListView::setItemBox(int index, std::shared_ptr<RenderBox> box)
    {
        auto it = item_boxes_.find(index);
        if (it != item_boxes_.end() && it->second.box)
            it->second.box->setParent(nullptr);

        if (box) box->setParent(this);
        item_boxes_[index] = {std::move(box), Offset::zero()};
        markNeedsLayout();
    }

    void RenderListView::removeItemBox(int index)
    {
        auto it = item_boxes_.find(index);
        if (it != item_boxes_.end())
        {
            if (it->second.box) it->second.box->setParent(nullptr);
            item_boxes_.erase(it);
        }
        markNeedsLayout();
    }

    int RenderListView::firstVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0) return 0;
        const float scroll = scrollOffset();
        return std::max(0, static_cast<int>(scroll / item_extent));
    }

    int RenderListView::lastVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0) return -1;
        const float scroll = scrollOffset();
        const int   last   = static_cast<int>((scroll + viewport_extent_) / item_extent);
        return std::min(item_count - 1, last);
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderListView::performLayout()
    {
        const bool is_v = (scroll_axis == Axis::vertical);

        size_ = constraints_.constrain({
            constraints_.max_width,
            constraints_.max_height,
        });

        viewport_extent_ = is_v ? size_.height : size_.width;

        const float content_extent = static_cast<float>(item_count) * item_extent;
        min_extent_ = 0.0f;
        max_extent_ = std::max(0.0f, content_extent - viewport_extent_);

        if (controller_)
        {
            controller_->setExtents(min_extent_, max_extent_);
        }
        else
        {
            internal_offset_ = std::clamp(internal_offset_, min_extent_, max_extent_);
        }

        const float scroll       = scrollOffset();
        const float cross_extent = is_v ? size_.width : size_.height;

        for (auto& [idx, entry] : item_boxes_)
        {
            if (!entry.box) continue;

            const float item_pos = static_cast<float>(idx) * item_extent;
            const BoxConstraints child_cc = is_v
                ? BoxConstraints::tight(cross_extent, item_extent)
                : BoxConstraints::tight(item_extent, cross_extent);

            entry.box->layout(child_cc);
            entry.offset = is_v
                ? Offset{0.0f, item_pos}
                : Offset{item_pos, 0.0f};
        }

        (void)scroll;
        checkVisibleRangeChanged();
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderListView::performPaint(PaintContext& context, const Offset& offset)
    {
        if (item_boxes_.empty()) return;

        const float scroll = scrollOffset();
        const bool  is_v   = (scroll_axis == Axis::vertical);

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));
        canvas.translate(is_v ? 0.0f : -scroll, is_v ? -scroll : 0.0f);

        for (const auto& [idx, entry] : item_boxes_)
        {
            if (entry.box)
                entry.box->paint(context, offset + entry.offset);
        }

        canvas.restore();
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    bool RenderListView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        const float  scroll   = scrollOffset();
        const bool   is_v     = (scroll_axis == Axis::vertical);
        const Offset adjusted = is_v
            ? Offset{position.x, position.y + scroll}
            : Offset{position.x + scroll, position.y};

        std::vector<const ChildEntry*> entries;
        entries.reserve(item_boxes_.size());
        for (const auto& [idx, entry] : item_boxes_)
            entries.push_back(&entry);

        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (!(*it)->box) continue;
            if ((*it)->box->hitTest(result, adjusted - (*it)->offset))
                return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    float RenderListView::scrollOffset() const noexcept
    {
        return controller_ ? controller_->offset() : internal_offset_;
    }

    void RenderListView::applyScrollDelta(float delta)
    {
        const float raw     = scrollOffset() + delta;
        const float clamped = physics_->applyBoundaryConditions(raw, min_extent_, max_extent_);

        if (controller_)
        {
            controller_->jumpTo(clamped);
        }
        else
        {
            if (clamped == internal_offset_) return;
            internal_offset_ = clamped;
            markNeedsPaint();
        }

        checkVisibleRangeChanged();
    }

    void RenderListView::checkVisibleRangeChanged()
    {
        const int first = firstVisibleIndex();
        const int last  = lastVisibleIndex();

        if (first != cached_first_ || last != cached_last_)
        {
            cached_first_ = first;
            cached_last_  = last;
            if (on_visible_range_changed) on_visible_range_changed();
        }
    }

    // -------------------------------------------------------------------------
    // Input handling
    // -------------------------------------------------------------------------

    void RenderListView::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pointer_down_  = true;
            panning_       = false;
            pan_last_pos_  = event.position;
            velocity_px_s_ = 0.0f;
            pan_velocity_  = 0.0f;
            last_pan_time_ = std::chrono::steady_clock::now();
            break;

        case PointerEventKind::move:
        {
            if (!pointer_down_)
                break;

            const bool  is_v = (scroll_axis == Axis::vertical);
            const float dx   = event.position.x - pan_last_pos_.x;
            const float dy   = event.position.y - pan_last_pos_.y;

            if (!panning_ && std::sqrt(dx * dx + dy * dy) > kTapSlop)
                panning_ = true;

            if (panning_)
            {
                const float delta = is_v ? dy : dx;
                applyScrollDelta(-delta);

                const auto  now = std::chrono::steady_clock::now();
                const float dt  = std::chrono::duration<float>(now - last_pan_time_).count();
                if (dt > 1e-4f) pan_velocity_ = -delta / dt;
                last_pan_time_ = now;
            }

            pan_last_pos_ = event.position;
            break;
        }

        case PointerEventKind::up:
            pointer_down_ = false;
            if (panning_ && physics_->allowsMomentum())
                velocity_px_s_ = pan_velocity_;
            panning_ = false;
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            break;

        case PointerEventKind::scroll:
        {
            const bool is_v = (scroll_axis == Axis::vertical);
            applyScrollDelta(is_v ? event.scroll_delta_y : event.scroll_delta_x);
            break;
        }
        }
    }

    void RenderListView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        const float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        const float current = scrollOffset();

        if (current < min_extent_ || current > max_extent_)
        {
            const float target   = std::clamp(current, min_extent_, max_extent_);
            const float spring_v = (target - current) * kSpringCoeff;
            velocity_px_s_       = spring_v + velocity_px_s_ * 0.7f;
            applyScrollDelta(velocity_px_s_ * dt_s);
            return;
        }

        if (std::abs(velocity_px_s_) < kMinVelocity) { velocity_px_s_ = 0.0f; return; }

        applyScrollDelta(velocity_px_s_ * dt_s);
        velocity_px_s_ = physics_->applyFriction(velocity_px_s_, dt_s);
    }


    void RenderListView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : item_boxes_)
            if (c.second.box.get()) visitor(c.second.box.get());
    }
} // namespace systems::leal::campello_widgets
