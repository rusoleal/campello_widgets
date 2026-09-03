#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <campello_widgets/ui/render_grid_view.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderGridView::RenderGridView()
    {
        physics_ = std::make_shared<ClampingScrollPhysics>();
    }

    RenderGridView::~RenderGridView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (controller_) controller_->detach();
    }

    void RenderGridView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderGridView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderGridView::setController(std::shared_ptr<ScrollController> controller)
    {
        if (controller_) controller_->detach();
        controller_ = std::move(controller);
        if (controller_) controller_->attach();
    }

    void RenderGridView::setPhysics(std::shared_ptr<ScrollPhysics> physics)
    {
        physics_ = physics ? std::move(physics) : std::make_shared<ClampingScrollPhysics>();
    }

    void RenderGridView::setItemBox(int index, std::shared_ptr<RenderBox> box)
    {
        auto it = item_boxes_.find(index);
        if (it != item_boxes_.end() && it->second.box)
            it->second.box->setParent(nullptr);

        if (box) box->setParent(this);
        item_boxes_[index] = {std::move(box), Offset::zero()};
        markNeedsLayout();
    }

    void RenderGridView::removeItemBox(int index)
    {
        auto it = item_boxes_.find(index);
        if (it != item_boxes_.end())
        {
            if (it->second.box) it->second.box->setParent(nullptr);
            item_boxes_.erase(it);
        }
        markNeedsLayout();
    }

    int RenderGridView::firstVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0 || cross_axis_count <= 0) return 0;
        // Clamp the *input* to the boundary, not just the resulting index.
        // BouncingScrollPhysics only resists overscroll, it doesn't cap it,
        // so scrollOffset() can sit well past [min_extent_, max_extent_]
        // during spring-back. Feeding that raw value in here would make the
        // visible-range keep changing every tick throughout the whole
        // bounce (edge items mounting/unmounting on a churn that has no
        // visual purpose — nothing new is ever revealed beyond the first or
        // last item), which shows up as items flickering at the boundary.
        // Pinning the range to what's visible exactly at the boundary keeps
        // it constant for the entire overscroll; only the paint offset
        // (which reads the real, unclamped scrollOffset()) moves.
        const float scroll    = std::clamp(scrollOffset(), min_extent_, max_extent_);
        const int   first_row = static_cast<int>(scroll / item_extent);
        return std::min(item_count - 1, std::max(0, first_row) * cross_axis_count);
    }

    int RenderGridView::lastVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0 || cross_axis_count <= 0) return -1;
        // See firstVisibleIndex()'s comment — same input clamp.
        const float scroll   = std::clamp(scrollOffset(), min_extent_, max_extent_);
        const int   last_row = static_cast<int>((scroll + viewport_height_) / item_extent);
        return std::min(item_count - 1, (last_row + 1) * cross_axis_count - 1);
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderGridView::performLayout()
    {
        size_ = constraints_.constrain({
            constraints_.max_width,
            constraints_.max_height,
        });

        viewport_height_ = size_.height;
        item_width_      = (cross_axis_count > 0)
            ? size_.width / static_cast<float>(cross_axis_count)
            : size_.width;

        // Guard against an unbounded/degenerate width reaching here — e.g. an
        // ancestor that hands GridView an infinite max_width (a horizontally-
        // unbounded container) divides through to an infinite item_width_,
        // which then poisons every mounted cell's tight layout constraint
        // below. If that cell is wrapped in a ClipRRect/ClipOval (as this
        // gallery's GridView cells are), the infinite bounds reaches
        // Renderer::applyClipShape() and corrupts GPU offscreen-texture
        // creation instead of failing gracefully. Clamping to 0 here means
        // cells simply don't lay out this pass rather than crashing.
        if (!std::isfinite(item_width_) || item_width_ < 0.0f)
            item_width_ = 0.0f;
        if (!std::isfinite(viewport_height_) || viewport_height_ < 0.0f)
            viewport_height_ = 0.0f;

        const int   total_rows     = (item_count > 0 && cross_axis_count > 0)
            ? (item_count + cross_axis_count - 1) / cross_axis_count
            : 0;
        const float content_height = static_cast<float>(total_rows) * item_extent;

        min_extent_ = 0.0f;
        max_extent_ = std::max(0.0f, content_height - viewport_height_);

        if (controller_)
        {
            controller_->setExtents(min_extent_, max_extent_);
        }
        else
        {
            internal_offset_ = std::clamp(internal_offset_, min_extent_, max_extent_);
            raw_offset_       = internal_offset_;
        }

        // Position each mounted item box.
        for (auto& [idx, entry] : item_boxes_)
        {
            if (!entry.box) continue;

            const int   row = (cross_axis_count > 0) ? idx / cross_axis_count : 0;
            const int   col = (cross_axis_count > 0) ? idx % cross_axis_count : 0;

            entry.box->layout(BoxConstraints::tight(item_width_, item_extent));
            entry.offset = {static_cast<float>(col) * item_width_,
                            static_cast<float>(row) * item_extent};
        }

        checkVisibleRangeChanged();
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderGridView::performPaint(PaintContext& context, const Offset& offset)
    {
        if (item_boxes_.empty()) return;

        const float scroll = scrollOffset();

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));
        canvas.translate(0.0f, -scroll);

        for (const auto& [idx, entry] : item_boxes_)
        {
            if (entry.box)
                entry.box->paint(context, offset + entry.offset);
        }

        canvas.restore();
    }

    void RenderGridView::paint(PaintContext& context, const Offset& offset)
    {
        // OR needsDescendantPaint() in: a replay skips performPaint()
        // entirely, so a nested boundary further down (e.g. a ClipRRect
        // thumbnail in a grid item) must not be silently stranded — see
        // that flag's doc comment.
        if (!offset_layer_.maybeReplay(context, offset, size_,
                                        needsPaint(), needsDescendantPaint()))
            offset_layer_.record(context, offset, [&] { performPaint(context, offset); });

        needs_paint_ = false;
        needs_descendant_paint_ = false;
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    bool RenderGridView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        const Offset adjusted = {position.x, position.y + scrollOffset()};

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

    float RenderGridView::scrollOffset() const noexcept
    {
        return controller_ ? controller_->offset() : internal_offset_;
    }

    void RenderGridView::applyScrollDelta(float delta, const char* source)
    {
        // See RenderListView::applyScrollDelta()'s doc — raw_offset_ is
        // the single source of truth mutated here; the displayed offset
        // is always a fresh function of it via the physics, never fed
        // back into itself.
        const float old_offset = scrollOffset();
        raw_offset_ += delta;
        // See RenderListView::applyScrollDelta()'s doc on wheel_velocity_tracker_.
        if (std::strcmp(source, "wheel") == 0)
            wheel_velocity_tracker_.addPosition(last_pointer_timestamp_ms_, raw_offset_);
        const float clamped = physics_->applyBoundaryConditions(raw_offset_, min_extent_, max_extent_);

        if (DebugFlags::printScrollTrace)
        {
            const float applied = clamped - old_offset;
            // See RenderListView::applyScrollDelta()'s doc — same jump
            // heuristic: boundary resistance may only shrink |applied|
            // relative to |delta| or leave it unchanged, never grow it or
            // flip its sign.
            const bool jump = (std::abs(delta) > 1e-4f &&
                                   (std::abs(applied) > std::abs(delta) + 0.5f ||
                                    (applied * delta < 0.0f && std::abs(applied) > 0.5f))) ||
                               (std::abs(delta) <= 1e-4f && std::abs(applied) > 0.5f);
            std::fprintf(stderr,
                "[scroll:grid] %-9s old=%8.2f delta=%8.2f raw_off=%8.2f clamped=%8.2f applied=%8.2f%s\n",
                source, old_offset, delta, raw_offset_, clamped, applied, jump ? "  <-- JUMP" : "");
        }

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

    void RenderGridView::checkVisibleRangeChanged()
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

    void RenderGridView::acceptGesture(int32_t /*pointer_id*/)
    {
        // Only records the win. panning_ starts once a move actually exceeds
        // pan slop (see onPointerEvent) — accepting here just means we're no
        // longer competing, not that movement has happened yet (e.g. a sole,
        // uncontested arena resolves immediately at pointer-down).
        won_arena_ = true;
    }

    void RenderGridView::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        panning_    = false;
    }

    void RenderGridView::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
            pointer_down_  = true;
            panning_       = false;
            won_arena_     = false;
            lost_arena_    = false;
            pan_last_pos_  = event.position;
            pan_down_pos_  = event.position;
            device_kind_   = event.device_kind;
            velocity_px_s_ = 0.0f;
            last_pointer_timestamp_ms_ = event.timestamp_ms;
            velocity_tracker_.reset();
            velocity_tracker_.addPosition(event.timestamp_ms, event.position.y);
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            break;

        case PointerEventKind::move:
        {
            if (!pointer_down_ || lost_arena_)
                break;

            const float dy = event.position.y - pan_last_pos_.y;

            // The slop check measures cumulative distance from pointer-down
            // (pan_down_pos_, fixed for the whole gesture), NOT the
            // frame-to-frame delta (dy above) — touch delivery arrives in
            // many small increments, so checking each one individually
            // against the slop threshold means a slow-building drag whose
            // per-frame steps never individually exceed it would never start
            // panning at all, no matter how far the finger travels in total.
            // GridView always scrolls vertically — only that axis counts
            // toward the pan-slop threshold, not total Euclidean movement,
            // so a horizontally-scrolling sibling (e.g. a nested ListView)
            // can still claim horizontal drags. Mirrors Flutter's
            // VerticalDragGestureRecognizer, which likewise measures only
            // vertical displacement, not the diagonal distance.
            const float total_dy = event.position.y - pan_down_pos_.y;

            if (!panning_ && std::abs(total_dy) > computePanSlop(device_kind_))
            {
                if (won_arena_)
                {
                    panning_ = true;
                }
                else if (arena_entry_)
                {
                    arena_entry_->resolve(GestureDisposition::accepted);
                    panning_ = true;
                }
            }

            last_pointer_timestamp_ms_ = event.timestamp_ms;
            if (panning_)
            {
                applyScrollDelta(-dy);
                velocity_tracker_.addPosition(event.timestamp_ms, event.position.y);
            }

            pan_last_pos_ = event.position;
            break;
        }

        case PointerEventKind::up:
            pointer_down_ = false;
            if (panning_ && physics_->allowsMomentum())
                velocity_px_s_ = -velocity_tracker_.getVelocity();
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
            // Registered via the signal resolver, not applied directly —
            // see PointerSignalResolver's doc. GridView always scrolls
            // vertically; mark `dominant` when vertical is the larger
            // component, so a horizontal sibling (e.g. a nested ListView)
            // isn't starved by GridView winning the resolver on every
            // swipe regardless of direction. Always register (not only
            // when dominant) so the resolver's fallback tier still applies
            // this delta when GridView is the sole scrollable in the hit
            // path — otherwise trackpad noise on the cross axis would
            // silently drop some events, felt as jumpy scrolling.
            const float delta    = event.scroll_delta_y;
            const bool  dominant = std::abs(event.scroll_delta_y) >= std::abs(event.scroll_delta_x);
            last_pointer_timestamp_ms_ = event.timestamp_ms;
            // See RenderListView::onPointerEvent()'s scroll case doc —
            // while overscrolled, drop trailing sub-threshold deltas
            // (macOS's own momentum-tail trickle) entirely rather than
            // applying them, or they'd fight the spring easing back and
            // the list would never quite settle exactly at the limit.
            const bool overscrolled = (raw_offset_ < min_extent_ || raw_offset_ > max_extent_);
            if (overscrolled && std::abs(delta) < kSignificantScrollDelta)
                break;
            if (auto* d = PointerDispatcher::activeDispatcher())
            {
                // Only a meaningful delta refreshes the "still actively
                // scrolling" gate, so spring-back isn't held hostage by
                // the OS's own rapidly-decaying momentum-tail trickle.
                if (std::abs(delta) >= kSignificantScrollDelta)
                {
                    last_scroll_event_ms_ = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    wheel_momentum_pending_ = true;
                }
                d->signalResolver().registerHandler([this, delta] { applyScrollDelta(delta, "wheel"); }, dominant);
            }
            break;
        }
        }
    }

    void RenderGridView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        // The OS is still actively delivering trackpad/wheel scroll events
        // for this gesture (including its own momentum tail) — let those
        // keep driving the (resistance-damped) overscroll; spring-back
        // would otherwise fight them every tick. See
        // last_scroll_event_ms_'s doc.
        if (now_ms - last_scroll_event_ms_ < kScrollActiveWindowMs)
        {
            last_tick_ms_ = now_ms;
            // See RenderListView::onTick()'s doc — must call
            // FrameScheduler::scheduleFrame() directly, not
            // markNeedsPaint(), which no-ops here since needs_paint_ is
            // already true (this frame is happening because of it) and
            // won't clear until this frame's own paint() runs, after
            // onTick already returned.
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            return;
        }

        // See RenderListView::onTick()'s doc on wheel_momentum_pending_.
        if (wheel_momentum_pending_)
        {
            wheel_momentum_pending_ = false;
            if (physics_->allowsMomentum())
                velocity_px_s_ = wheel_velocity_tracker_.getVelocity();
        }

        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        const float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        // Spring back when overscrolled (BouncingScrollPhysics use case).
        // Eases raw_offset_ — the same single source of truth
        // applyScrollDelta() mutates — not the displayed/resisted offset;
        // see applyScrollDelta()'s doc for why re-resisting an
        // already-resisted value compounds incorrectly.
        if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_)
        {
            const float target = std::clamp(raw_offset_, min_extent_, max_extent_);
            // Exponential ease of the overscroll distance toward zero —
            // unconditionally stable regardless of dt/frame rate, unlike
            // a velocity-based spring (`velocity = k*displacement +
            // velocity*damping`), which can accumulate velocity faster
            // than the position converges and overshoot past the target
            // with real residual speed — carrying that speed into the
            // momentum phase below and bouncing off the *opposite* edge,
            // felt as sustained vibration rather than a settle.
            const float eased_overscroll = (raw_offset_ - target) * std::exp(-kSpringCoeff * dt_s);
            // Snap once close enough — see kSpringSettleThreshold's doc.
            const float new_raw = (std::abs(eased_overscroll) < kSpringSettleThreshold)
                ? target : (target + eased_overscroll);
            applyScrollDelta(new_raw - raw_offset_, "spring");
            velocity_px_s_ = 0.0f;
            // See RenderListView::onTick()'s doc — nothing else asks for a
            // follow-up frame once this one finishes painting.
            FrameScheduler::scheduleFrame();
            return;
        }

        if (std::abs(velocity_px_s_) < kMinVelocity) { velocity_px_s_ = 0.0f; return; }

        applyScrollDelta(velocity_px_s_ * dt_s, "momentum");
        velocity_px_s_ = physics_->applyFriction(velocity_px_s_, dt_s);
        if (std::abs(velocity_px_s_) >= kMinVelocity)
            FrameScheduler::scheduleFrame();
    }


    void RenderGridView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : item_boxes_)
            if (c.second.box.get()) visitor(c.second.box.get());
    }
} // namespace systems::leal::campello_widgets
