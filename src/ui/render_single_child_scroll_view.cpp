#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderSingleChildScrollView::RenderSingleChildScrollView()
    {
        physics_ = std::make_shared<ClampingScrollPhysics>();
    }

    RenderSingleChildScrollView::~RenderSingleChildScrollView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (controller_) controller_->detach();
    }

    void RenderSingleChildScrollView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderSingleChildScrollView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderSingleChildScrollView::setController(
        std::shared_ptr<ScrollController> controller)
    {
        if (controller_) controller_->detach();
        controller_ = std::move(controller);
        if (controller_) controller_->attach();
    }

    void RenderSingleChildScrollView::setPhysics(std::shared_ptr<ScrollPhysics> physics)
    {
        physics_ = physics ? std::move(physics) : std::make_shared<ClampingScrollPhysics>();
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderSingleChildScrollView::performLayout()
    {
        if (!child_)
        {
            size_       = constraints_.constrain(Size::zero());
            min_extent_ = 0.0f;
            max_extent_ = 0.0f;
            return;
        }

        const bool  is_v = (scroll_axis == Axis::vertical);
        const float inf  = std::numeric_limits<float>::infinity();

        const BoxConstraints child_cc = is_v
            ? BoxConstraints{constraints_.min_width, constraints_.max_width, 0.0f, inf}
            : BoxConstraints{0.0f, inf, constraints_.min_height, constraints_.max_height};

        const Size child_size = layoutChild(*child_, child_cc);

        size_ = constraints_.constrain({
            is_v ? child_size.width  : constraints_.max_width,
            is_v ? constraints_.max_height : child_size.height,
        });

        const float viewport = is_v ? size_.height : size_.width;
        const float content  = is_v ? child_size.height : child_size.width;

        min_extent_ = 0.0f;
        max_extent_ = std::max(0.0f, content - viewport);

        if (controller_)
        {
            controller_->setExtents(min_extent_, max_extent_);
        }
        else
        {
            internal_offset_ = std::clamp(internal_offset_, min_extent_, max_extent_);
            raw_offset_       = internal_offset_;
        }

        child_offset_ = Offset::zero();
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderSingleChildScrollView::performPaint(
        PaintContext& context, const Offset& offset)
    {
        if (!child_) return;

        const float scroll = scrollOffset();
        const bool  is_v   = (scroll_axis == Axis::vertical);

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));
        canvas.translate(is_v ? 0.0f : -scroll, is_v ? -scroll : 0.0f);
        child_->paint(context, offset);
        canvas.restore();
    }

    void RenderSingleChildScrollView::paint(PaintContext& context, const Offset& offset)
    {
        // Note: performPaint() always clips to its own viewport rect above,
        // so a repositioned cache always contains unsafe geometry and falls
        // back to a full re-record — see OffsetLayer's doc comment. Cheap
        // delta-translate reposition never triggers for this class; it's
        // still worth composing OffsetLayer here for the identity-replay
        // path (an unrelated repaint elsewhere doesn't force this content
        // to be re-walked) and to share one caching mechanism framework-wide.
        // OR needsDescendantPaint() in: a replay skips performPaint()/
        // paintChild() entirely, so a nested boundary further down (e.g. a
        // ClipRRect avatar in scrolled content) must not be silently
        // stranded — see that flag's doc comment.
        if (!offset_layer_.maybeReplay(context, offset, size_,
                                        needsPaint(), needsDescendantPaint()))
            offset_layer_.record(context, offset, [&] { performPaint(context, offset); });

        needs_paint_ = false;
        needs_descendant_paint_ = false;
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    bool RenderSingleChildScrollView::hitTestChildren(
        HitTestResult& result, const Offset& position)
    {
        if (!child_) return false;

        const float  scroll   = scrollOffset();
        const bool   is_v     = (scroll_axis == Axis::vertical);
        const Offset adjusted = is_v
            ? Offset{position.x, position.y + scroll}
            : Offset{position.x + scroll, position.y};

        return child_->hitTest(result, adjusted);
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    float RenderSingleChildScrollView::scrollOffset() const noexcept
    {
        return controller_ ? controller_->offset() : internal_offset_;
    }

    void RenderSingleChildScrollView::applyScrollDelta(float delta, const char* source)
    {
        // See RenderListView::applyScrollDelta()'s doc — raw_offset_ is
        // the single source of truth mutated here; the displayed offset
        // is always a fresh function of it via the physics, never fed
        // back into itself.
        const float old_offset = scrollOffset();
        raw_offset_ += delta;
        // See RenderListView::applyScrollDelta()'s doc on wheel_velocity_tracker_.
        if (std::strcmp(source, "wheel") == 0)
            wheel_velocity_tracker_.addPosition(std::chrono::steady_clock::now(), raw_offset_);
        const float clamped = physics_->applyBoundaryConditions(raw_offset_, min_extent_, max_extent_);

        if (DebugFlags::printScrollTrace)
        {
            const float applied = clamped - old_offset;
            const bool jump = (std::abs(delta) > 1e-4f &&
                                   (std::abs(applied) > std::abs(delta) + 0.5f ||
                                    (applied * delta < 0.0f && std::abs(applied) > 0.5f))) ||
                               (std::abs(delta) <= 1e-4f && std::abs(applied) > 0.5f);
            std::fprintf(stderr,
                "[scroll:scr]  %-9s old=%8.2f delta=%8.2f raw_off=%8.2f clamped=%8.2f applied=%8.2f%s\n",
                source, old_offset, delta, raw_offset_, clamped, applied, jump ? "  <-- JUMP" : "");
        }

        if (controller_)
        {
            // See RenderListView::applyScrollDelta()'s doc -- read
            // controller_->offset() BEFORE jumpTo() mutates it. Nothing
            // here previously requested a repaint at all when a
            // controller is attached; it relied on some listener
            // happening to setState/rebuild as a side effect of
            // controller_->notifyListeners(), which is fragile and
            // produces visible stepping whenever nothing else happens to
            // be listening on every tick.
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
    }

    float RenderSingleChildScrollView::applyExternalScrollDelta(float delta)
    {
        const float before = scrollOffset();
        applyScrollDelta(delta, "external");
        return scrollOffset() - before;
    }

    // -------------------------------------------------------------------------
    // Input handling
    // -------------------------------------------------------------------------

    void RenderSingleChildScrollView::acceptGesture(int32_t /*pointer_id*/)
    {
        // Only records the win. panning_ starts once a move actually exceeds
        // pan slop (see onPointerEvent) — accepting here just means we're no
        // longer competing, not that movement has happened yet (e.g. a sole,
        // uncontested arena resolves immediately at pointer-down).
        won_arena_ = true;
    }

    void RenderSingleChildScrollView::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        panning_    = false;
    }

    void RenderSingleChildScrollView::onPointerEvent(const PointerEvent& event)
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
            velocity_px_s_ = 0.0f;
            velocity_tracker_.reset();
            velocity_tracker_.addPosition(
                std::chrono::steady_clock::now(),
                scroll_axis == Axis::vertical ? event.position.y : event.position.x);
            device_kind_   = event.device_kind;
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            break;

        case PointerEventKind::move:
        {
            if (!pointer_down_ || lost_arena_)
                break;

            const bool  is_v  = (scroll_axis == Axis::vertical);
            const float dx    = event.position.x - pan_last_pos_.x;
            const float dy    = event.position.y - pan_last_pos_.y;

            // The slop check measures cumulative distance from pointer-down
            // (pan_down_pos_, fixed for the whole gesture), NOT the
            // frame-to-frame delta (dx/dy above) — touch delivery arrives in
            // many small increments, so checking each one individually
            // against the slop threshold means a slow-building drag whose
            // per-frame steps never individually exceed it would never start
            // panning at all, no matter how far the finger travels in total.
            // Only this view's own scroll axis counts toward the threshold —
            // not total Euclidean movement — so a scrollable nested inside
            // another with a different axis only claims the gesture arena
            // for drags actually aligned with its own axis. Mirrors
            // Flutter's VerticalDragGestureRecognizer/
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
                    arena_entry_->resolve(GestureDisposition::accepted);
                    panning_ = true;
                }
            }

            if (panning_)
            {
                const float delta = is_v ? dy : dx;
                applyScrollDelta(-delta); // drag up → scroll down
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
            // event, so a genuine cross-axis swipe still routes to a
            // nested scrollable on the other axis. Registering
            // unconditionally (rather than skipping when non-dominant)
            // matters when this is the *only* scrollable in the hit path —
            // the resolver's fallback tier ensures the event still applies
            // instead of silently dropping on trackpad noise.
            const bool  is_v  = (scroll_axis == Axis::vertical);
            const float delta       = is_v ? event.scroll_delta_y : event.scroll_delta_x;
            const float cross_delta = is_v ? event.scroll_delta_x : event.scroll_delta_y;
            const bool  dominant    = std::abs(delta) >= std::abs(cross_delta);
            // See RenderListView::onPointerEvent()'s scroll case doc —
            // while overscrolled, drop trailing sub-threshold deltas
            // (macOS's own momentum-tail trickle) entirely rather than
            // applying them, or they'd fight the spring easing back and
            // the view would never quite settle exactly at the limit.
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

    void RenderSingleChildScrollView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        // The OS is still actively delivering trackpad/wheel scroll events
        // for this gesture (including its own momentum tail) — let those
        // keep driving the (resistance-damped) overscroll; spring-back
        // would otherwise fight them every tick. See last_scroll_event_ms_'s
        // doc.
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

        if (std::abs(velocity_px_s_) < kMinVelocity)
        {
            velocity_px_s_ = 0.0f;
            return;
        }

        applyScrollDelta(velocity_px_s_ * dt_s, "momentum");
        velocity_px_s_ = physics_->applyFriction(velocity_px_s_, dt_s);
        if (std::abs(velocity_px_s_) >= kMinVelocity)
            FrameScheduler::scheduleFrame();
    }

} // namespace systems::leal::campello_widgets
