#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
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
            d->arena().removeMember(this);
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
        const float scroll = std::clamp(scrollOffset(), min_extent_, max_extent_);
        return std::max(0, static_cast<int>(scroll / item_extent));
    }

    int RenderListView::lastVisibleIndex() const noexcept
    {
        if (item_extent <= 0.0f || item_count <= 0) return -1;
        // See firstVisibleIndex()'s comment — same input clamp.
        const float scroll = std::clamp(scrollOffset(), min_extent_, max_extent_);
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
            raw_offset_       = internal_offset_;
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

    void RenderListView::paint(PaintContext& context, const Offset& offset)
    {
        // OR needsDescendantPaint() in: a replay skips performPaint()
        // entirely, so a nested boundary further down (e.g. a ClipRRect
        // avatar in a list item) must not be silently stranded — see that
        // flag's doc comment.
        if (!offset_layer_.maybeReplay(context, offset, size_,
                                        needsPaint(), needsDescendantPaint()))
            offset_layer_.record(context, offset, [&] { performPaint(context, offset); });

        needs_paint_ = false;
        needs_descendant_paint_ = false;
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

    void RenderListView::applyScrollDelta(float delta, const char* source)
    {
        // raw_offset_ is the single source of truth being mutated here —
        // the true, unresisted cumulative position. The displayed offset
        // is always a fresh function of it via the physics, never fed
        // back into itself: `ScrollPhysics::applyBoundaryConditions()` is
        // a position-remapping formula (over = distance past boundary,
        // displayed = boundary ± over * resistance), and remapping an
        // already-remapped value compounds the resistance each call,
        // which can *retract* the displayed overscroll even while the
        // user keeps dragging further in the same direction whenever the
        // per-event delta shrinks (see the regression this fixed). Easing
        // raw_offset_ itself during spring-back (onTick) keeps the same
        // single source of truth for that phase too.
        const float old_offset = scrollOffset();
        raw_offset_ += delta;
        // Feed the wheel's own observed velocity into our tracker so onTick
        // can hand off to app-driven momentum the moment the OS stops
        // sending scroll events — see onTick()'s doc for why relying
        // solely on the OS's own (often short) kinetic-scroll tail isn't
        // enough. Position, not delta: the tracker fits a line through
        // samples, so it needs the cumulative value.
        if (std::strcmp(source, "wheel") == 0)
            wheel_velocity_tracker_.addPosition(std::chrono::steady_clock::now(), raw_offset_);
        const float clamped = physics_->applyBoundaryConditions(raw_offset_, min_extent_, max_extent_);

        if (DebugFlags::printScrollTrace)
        {
            const float applied = clamped - old_offset;
            // Boundary resistance may only ever shrink |applied| relative
            // to |delta| (elastic damping) or leave it unchanged (in
            // bounds) — it should never grow it, and it should never flip
            // its sign. Either of those means something moved the offset
            // by more than this call's own input explains.
            const bool jump = (std::abs(delta) > 1e-4f &&
                                   (std::abs(applied) > std::abs(delta) + 0.5f ||
                                    (applied * delta < 0.0f && std::abs(applied) > 0.5f))) ||
                               (std::abs(delta) <= 1e-4f && std::abs(applied) > 0.5f);
            std::fprintf(stderr,
                "[scroll:list] %-9s old=%8.2f delta=%8.2f raw_off=%8.2f clamped=%8.2f applied=%8.2f%s\n",
                source, old_offset, delta, raw_offset_, clamped, applied, jump ? "  <-- JUMP" : "");
        }

        if (controller_)
        {
            // controller_->offset() only updates when the clamped value
            // actually changes (ScrollController::setOffset()'s own
            // dedup) -- read it BEFORE jumpTo() so this comparison sees
            // the prior value, not the one we're about to set. Unlike the
            // no-controller branch below, nothing here previously
            // requested a repaint at all: it relied entirely on some
            // listener happening to setState/rebuild as a side effect of
            // controller_->notifyListeners() (e.g. Scrollbar's own
            // subscription) -- fragile, and produces exactly the
            // steps-of-several-pixels symptom this fixes when nothing
            // else happens to be listening on every tick.
            const bool offset_changed = (clamped != controller_->offset());
            controller_->jumpTo(clamped);
            controller_->notifyOverscroll(std::max(0.0f, min_extent_ - raw_offset_));
            if (offset_changed)
                markNeedsPaint();
        }
        else
        {
            if (clamped == internal_offset_) return;
            internal_offset_ = clamped;
            markNeedsPaint();
        }

        checkVisibleRangeChanged();
    }

    float RenderListView::applyExternalScrollDelta(float delta)
    {
        const float before = scrollOffset();
        applyScrollDelta(delta, "external");
        return scrollOffset() - before;
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
    // GestureArenaMember
    // -------------------------------------------------------------------------

    void RenderListView::acceptGesture(int32_t /*pointer_id*/)
    {
        // Only records the win. panning_ starts once a move actually exceeds
        // pan slop (see onPointerEvent) — accepting here just means we're no
        // longer competing, not that movement has happened yet (e.g. a sole,
        // uncontested arena resolves immediately at pointer-down).
        won_arena_ = true;
    }

    void RenderListView::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        panning_    = false;
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
            won_arena_     = false;
            lost_arena_    = false;
            pan_last_pos_  = event.position;
            pan_down_pos_  = event.position;
            device_kind_   = event.device_kind;
            velocity_px_s_ = 0.0f;
            velocity_tracker_.reset();
            velocity_tracker_.addPosition(
                std::chrono::steady_clock::now(),
                scroll_axis == Axis::vertical ? event.position.y : event.position.x);
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            break;

        case PointerEventKind::move:
        {
            if (!pointer_down_ || lost_arena_)
                break;

            const bool  is_v = (scroll_axis == Axis::vertical);
            const float dx   = event.position.x - pan_last_pos_.x;
            const float dy   = event.position.y - pan_last_pos_.y;

            // The slop check measures cumulative distance from pointer-down
            // (pan_down_pos_, fixed for the whole gesture), NOT the
            // frame-to-frame delta (dx/dy above, from pan_last_pos_, which
            // advances every move). Touch delivery arrives in many small
            // increments — checking each individual increment against the
            // slop threshold means a slow-building drag whose per-frame
            // steps never individually exceed the threshold would never
            // start panning at all, no matter how far the finger travels in
            // total. Only this list's own scroll axis counts toward the
            // threshold — not total Euclidean movement — so a horizontal
            // list nested inside a vertical one (or vice versa) only claims
            // the gesture arena for drags actually aligned with its own
            // axis. Mirrors Flutter's VerticalDragGestureRecognizer/
            // HorizontalDragGestureRecognizer, which likewise measure only
            // their own axis's displacement, not the diagonal distance.
            const float total_dx = event.position.x - pan_down_pos_.x;
            const float total_dy = event.position.y - pan_down_pos_.y;

            if (!panning_ && std::abs(is_v ? total_dy : total_dx) > computePanSlop(device_kind_))
            {
                if (won_arena_)
                {
                    panning_ = true;
                }
                else if (arena_entry_)
                {
                    // No competitor remains to reject us at this point (one
                    // would already have set lost_arena_ earlier), so this
                    // always resolves synchronously in our favor.
                    arena_entry_->resolve(GestureDisposition::accepted);
                    panning_ = true;
                }
            }

            if (panning_)
            {
                const float delta = is_v ? dy : dx;
                applyScrollDelta(-delta);
                velocity_tracker_.addPosition(
                    std::chrono::steady_clock::now(),
                    is_v ? event.position.y : event.position.x);
            }

            pan_last_pos_ = event.position;
            break;
        }

        case PointerEventKind::up:
            pointer_down_ = false;
            if (panning_ && physics_->allowsMomentum())
                // Negated to match applyScrollDelta(-delta)'s sign
                // convention in the move handler above — the tracker
                // reports the finger's own raw velocity, not the content's.
                velocity_px_s_ = -velocity_tracker_.getVelocity();
            panning_ = false;
            arena_entry_.reset();
            // Releasing while overscrolled (spring-back) or with residual
            // velocity (momentum) needs onTick() to keep running, but
            // nothing else requests that first follow-up frame on release
            // -- onTick() only fires once a frame is actually scheduled,
            // so without this, a release that leaves the view overscrolled
            // (e.g. a partial pull-to-refresh, cancelled before the
            // trigger threshold) freezes at the last dragged frame forever.
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            arena_entry_.reset();
            // See the `up` case's doc above -- same gap, same fix.
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            break;

        case PointerEventKind::scroll:
        {
            // Registered via the signal resolver, not applied directly —
            // see PointerSignalResolver's doc. Always register; mark
            // `dominant` when this axis is the larger component of the
            // event, so a genuine cross-axis swipe still routes to an
            // ancestor/sibling scrolling the other way. Registering
            // unconditionally (rather than skipping when non-dominant)
            // matters when this is the *only* scrollable in the hit path —
            // the resolver's fallback tier ensures the event still applies
            // instead of silently dropping on trackpad noise.
            const bool  is_v   = (scroll_axis == Axis::vertical);
            const float delta       = is_v ? event.scroll_delta_y : event.scroll_delta_x;
            const float cross_delta = is_v ? event.scroll_delta_x : event.scroll_delta_y;
            const bool  dominant    = std::abs(delta) >= std::abs(cross_delta);
            // While overscrolled, a trailing sub-threshold delta (macOS's
            // own rapidly-decaying momentum tail, which keeps sending
            // events for well over a second after the fingers lift) is
            // dropped entirely rather than applied — letting the spring
            // alone govern recovery. Applying it too (even without
            // refreshing the "still active" gate below) would still nudge
            // raw_offset_ away from the boundary on the very tick the
            // spring is easing it back, so the two fight and the list
            // never quite settles exactly at the limit. A delta at or
            // above the threshold is real, still-intentional input and
            // always applies normally, overscrolled or not.
            const bool overscrolled = (raw_offset_ < min_extent_ || raw_offset_ > max_extent_);
            if (overscrolled && std::abs(delta) < kSignificantScrollDelta)
                break;
            if (auto* d = PointerDispatcher::activeDispatcher())
            {
                // Only a *meaningful* delta counts as "still actively
                // scrolling" for the gate below — see the doc above for
                // why trailing OS momentum trickle shouldn't count.
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

    void RenderListView::onTick(uint64_t now_ms)
    {
        if (DebugFlags::printScrollTrace)
        {
            std::fprintf(stderr,
                "[tick:list]  now=%10llu panning=%d gate_ms=%6lld raw_off=%8.2f min=%8.2f max=%8.2f vel=%8.2f\n",
                static_cast<unsigned long long>(now_ms), panning_ ? 1 : 0,
                static_cast<long long>(now_ms - last_scroll_event_ms_),
                raw_offset_, min_extent_, max_extent_, velocity_px_s_);
        }
        if (panning_) { last_tick_ms_ = now_ms; return; }
        // The OS is still actively delivering trackpad/wheel scroll events
        // for this gesture (including its own momentum tail) — let those
        // keep driving the (resistance-damped) overscroll; spring-back
        // would otherwise fight them every tick. See
        // last_scroll_event_ms_'s doc.
        if (now_ms - last_scroll_event_ms_ < kScrollActiveWindowMs)
        {
            last_tick_ms_ = now_ms;
            // Frames on this platform are demand-driven (no free-running
            // vsync loop) — a frame is only built when something requests
            // one. If this is the *last* scroll event of the gesture, its
            // own triggered frame is the one hitting this early return
            // (now_ms is ~0ms past last_scroll_event_ms_), and nothing
            // else will ask for another frame once this window closes.
            // Without a self-requested frame here, a pending overscroll
            // would freeze forever instead of spring-back ever getting a
            // chance to run. Only bother when there's actually something
            // to resolve once the window clears.
            //
            // Must call FrameScheduler::scheduleFrame() directly, not
            // markNeedsPaint() — markNeedsPaint() no-ops whenever
            // needs_paint_ is already true (near-guaranteed here, since
            // that's *why* this frame is happening at all), so it can
            // never actually request the *next* frame from inside onTick,
            // which runs before this frame's own paint() call clears that
            // flag. scheduleFrame() itself has no such dedup state.
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            return;
        }

        // The active-scroll-window gate just closed for the first time
        // since the last wheel event — the OS has stopped delivering
        // trackpad/wheel scroll for this gesture. Hand off to our own
        // momentum using the velocity we observed from its recent deltas,
        // so the list keeps decelerating smoothly instead of stopping
        // exactly wherever the platform's own (possibly short) kinetic
        // tail happened to end.
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
            // Frames are demand-driven on this platform (see the comment
            // above on the trackpad-active branch) — nothing else asks for
            // a follow-up frame once this one finishes painting, so the
            // spring would freeze after a single step without this.
            FrameScheduler::scheduleFrame();
            return;
        }

        if (std::abs(velocity_px_s_) < kMinVelocity) { velocity_px_s_ = 0.0f; return; }

        applyScrollDelta(velocity_px_s_ * dt_s, "momentum");
        velocity_px_s_ = physics_->applyFriction(velocity_px_s_, dt_s);
        // Same reasoning as the spring-back branch above: without
        // self-requesting the next frame, momentum only ever applies once
        // — whatever frame was already in flight when the pointer lifted —
        // and then stops dead, looking like momentum isn't working at all.
        if (std::abs(velocity_px_s_) >= kMinVelocity)
            FrameScheduler::scheduleFrame();
    }


    void RenderListView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : item_boxes_)
            if (c.second.box.get()) visitor(c.second.box.get());
    }
} // namespace systems::leal::campello_widgets
