#include <algorithm>
#include <cmath>
#include <cstring>
#include <campello_widgets/ui/render_viewport.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    RenderViewport::RenderViewport() = default;

    RenderViewport::~RenderViewport()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (controller_) controller_->detach();
    }

    void RenderViewport::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderViewport::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->arena().removeMember(this);
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderViewport::setController(std::shared_ptr<ScrollController> controller)
    {
        if (controller_) controller_->detach();
        controller_ = std::move(controller);
        if (controller_) controller_->attach();
    }

    // -------------------------------------------------------------------------
    // Child management
    // -------------------------------------------------------------------------

    void RenderViewport::insertChild(std::shared_ptr<RenderSliver> sliver, int index)
    {
        if (index < 0) index = 0;
        if (index >= static_cast<int>(children_.size()))
            children_.resize(index + 1);

        auto& c = children_[index];

        // Same sliver already in this slot -- avoid needless setParent()
        // churn, matching RenderStack::insertChild()'s own reasoning
        // (render_stack.cpp:23-32).
        if (c.sliver == sliver) return;

        if (c.sliver) c.sliver->setParent(nullptr);
        c = ViewportChild{std::move(sliver), 0.0f};
        if (c.sliver) c.sliver->setParent(this);
        markNeedsLayout();
    }

    void RenderViewport::clearChildren()
    {
        for (auto& c : children_)
            if (c.sliver) c.sliver->setParent(nullptr);
        children_.clear();
        markNeedsLayout();
    }

    void RenderViewport::truncateChildren(size_t count)
    {
        if (count >= children_.size()) return;
        for (size_t i = count; i < children_.size(); ++i)
            if (children_[i].sliver) children_[i].sliver->setParent(nullptr);
        children_.resize(count);
        markNeedsLayout();
    }

    void RenderViewport::visitSliverChildren(const std::function<void(RenderSliver*)>& visitor) const
    {
        for (const auto& c : children_)
            if (c.sliver) visitor(c.sliver.get());
    }

    float RenderViewport::scrollOffset() const noexcept
    {
        return controller_ ? controller_->offset() : internal_offset_;
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderViewport::performLayout()
    {
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});
        viewport_extent_ = (axis == Axis::vertical) ? size_.height : size_.width;
        const float cross_extent = (axis == Axis::vertical) ? size_.width : size_.height;
        const float scroll = scrollOffset();

        // Matches Flutter's own bound -- a real guard against a buggy sliver
        // correcting forever, not just a stylistic choice.
        static constexpr int kMaxLayoutCycles = 10;

        float correction_total = 0.0f;
        for (int cycle = 0; ; ++cycle)
        {
            float preceding        = 0.0f;
            float max_paint_offset = 0.0f;
            // Running total of every earlier pinned obstruction's
            // max_scroll_obstruction_extent (Stage 5) -- 0 unless a pinned
            // persistent header precedes the current child. See
            // ViewportChild's own doc and RenderSliverPersistentHeader.
            float pin_floor = 0.0f;
            std::optional<float> pending_correction;

            for (auto& c : children_)
            {
                if (!c.sliver) continue;

                SliverConstraints sc{};
                sc.axis                      = axis;
                sc.growth_direction          = GrowthDirection::forward;
                sc.scroll_offset             = std::max(0.0f, scroll + correction_total - preceding);
                sc.preceding_scroll_extent   = preceding;
                sc.overlap                   = pin_floor;
                sc.remaining_paint_extent    = std::max(0.0f, viewport_extent_ - max_paint_offset);
                sc.remaining_cache_extent    = sc.remaining_paint_extent;
                sc.viewport_main_axis_extent = viewport_extent_;
                sc.cross_axis_extent         = cross_extent;

                c.sliver->layoutSliver(sc);
                const SliverGeometry& g = c.sliver->geometry();

                if (g.scroll_offset_correction.has_value())
                {
                    pending_correction = g.scroll_offset_correction;
                    break; // re-run the WHOLE pass with the adjusted offset
                }

                // A sliver that reports itself as a pinned obstruction (a
                // persistent header) has its natural, scroll-delta-based
                // offset clamped so it never scrolls above the floor
                // reserved by any earlier pinned obstruction -- this is the
                // entire pinning mechanism; see the Stage 5 plan's worked
                // example. Ordinary content (max_scroll_obstruction_extent
                // == 0, true for every pre-Stage-5 sliver) is completely
                // untouched -- natural_offset is used as-is.
                const float natural_offset = preceding - (scroll + correction_total);
                const bool  is_obstruction = g.max_scroll_obstruction_extent > 0.0f;
                c.layout_offset         = is_obstruction ? std::max(natural_offset, pin_floor) : natural_offset;
                c.is_pinned_obstruction = is_obstruction;
                c.clip_floor            = pin_floor;

                preceding       += g.scroll_extent;
                max_paint_offset = std::max(max_paint_offset, c.layout_offset + g.paint_extent);
                if (is_obstruction)
                    pin_floor = std::max(pin_floor, c.layout_offset + g.max_scroll_obstruction_extent);
            }

            if (pending_correction.has_value() && cycle < kMaxLayoutCycles)
            {
                correction_total += *pending_correction;
                continue;
            }

            min_extent_ = 0.0f;
            max_extent_ = std::max(0.0f, preceding - viewport_extent_);
            break;
        }

        if (controller_)
        {
            controller_->setExtents(min_extent_, max_extent_);
        }
        else
        {
            internal_offset_ = std::clamp(internal_offset_, min_extent_, max_extent_);
            raw_offset_       = internal_offset_;
        }
    }

    // -------------------------------------------------------------------------
    // Paint -- no ambient translate: SliverConstraints/SliverGeometry are
    // already viewport-relative by design, so c.layout_offset is already
    // resolved to visible-space (see the class doc / plan's paint section).
    //
    // Two passes (Stage 5): non-obstructing children first, each clipped to
    // its own clip_floor when a pinned header precedes it (see
    // performLayout()'s pin_floor bookkeeping and the Stage 5 plan's worked
    // example for why this clip is necessary once a header is fully
    // collapsed/pinned); pinned obstructions paint last, unclipped, so they
    // always render on top of whatever scrolled underneath them regardless
    // of insertion order. For a viewport with no pinned headers, every
    // clip_floor is 0 and the second pass is empty -- byte-identical to the
    // single-pass behavior before Stage 5.
    // -------------------------------------------------------------------------

    void RenderViewport::performPaint(PaintContext& context, const Offset& offset)
    {
        if (children_.empty()) return;

        const bool is_v = (axis == Axis::vertical);

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));

        for (const auto& c : children_)
        {
            if (!c.sliver || !c.sliver->geometry().visible || c.is_pinned_obstruction) continue;

            const Offset child_offset = is_v
                ? Offset{0.0f, c.layout_offset}
                : Offset{c.layout_offset, 0.0f};

            if (c.clip_floor > 0.0f)
            {
                canvas.save();
                canvas.clipRect(is_v
                    ? Rect::fromLTWH(offset.x, offset.y + c.clip_floor,
                                      size_.width, std::max(0.0f, size_.height - c.clip_floor))
                    : Rect::fromLTWH(offset.x + c.clip_floor, offset.y,
                                      std::max(0.0f, size_.width - c.clip_floor), size_.height));
                c.sliver->paint(context, offset + child_offset);
                canvas.restore();
            }
            else
            {
                c.sliver->paint(context, offset + child_offset);
            }
        }

        for (const auto& c : children_)
        {
            if (!c.sliver || !c.sliver->geometry().visible || !c.is_pinned_obstruction) continue;

            const Offset child_offset = is_v
                ? Offset{0.0f, c.layout_offset}
                : Offset{c.layout_offset, 0.0f};

            c.sliver->paint(context, offset + child_offset);
        }

        canvas.restore();
    }

    // -------------------------------------------------------------------------
    // GestureArenaMember
    // -------------------------------------------------------------------------

    void RenderViewport::acceptGesture(int32_t /*pointer_id*/)
    {
        won_arena_ = true;
    }

    void RenderViewport::rejectGesture(int32_t /*pointer_id*/)
    {
        lost_arena_ = true;
        panning_    = false;
    }

    // -------------------------------------------------------------------------
    // Scroll application -- field-for-field mirrors RenderListView::applyScrollDelta()
    // -------------------------------------------------------------------------

    void RenderViewport::applyScrollDelta(float delta, const char* source)
    {
        raw_offset_ += delta;
        if (std::strcmp(source, "wheel") == 0)
            wheel_velocity_tracker_.addPosition(std::chrono::steady_clock::now(), raw_offset_);
        const float clamped = physics->applyBoundaryConditions(raw_offset_, min_extent_, max_extent_);

        if (controller_)
        {
            const bool offset_changed = (clamped != controller_->offset());
            controller_->jumpTo(clamped);
            controller_->notifyOverscroll(std::max(0.0f, min_extent_ - raw_offset_));
            if (offset_changed)
                markNeedsLayout();
        }
        else
        {
            if (clamped == internal_offset_) return;
            internal_offset_ = clamped;
            markNeedsLayout();
        }
    }

    // -------------------------------------------------------------------------
    // Input handling -- field-for-field mirrors RenderListView::onPointerEvent()
    // -------------------------------------------------------------------------

    void RenderViewport::onPointerEvent(const PointerEvent& event)
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
                axis == Axis::vertical ? event.position.y : event.position.x);
            arena_entry_.reset();
            if (auto* d = PointerDispatcher::activeDispatcher())
                arena_entry_.emplace(d->arena().add(event.pointer_id, this));
            break;

        case PointerEventKind::move:
        {
            if (!pointer_down_ || lost_arena_)
                break;

            const bool  is_v = (axis == Axis::vertical);
            const float dx   = event.position.x - pan_last_pos_.x;
            const float dy   = event.position.y - pan_last_pos_.y;

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
            if (panning_ && physics->allowsMomentum())
                velocity_px_s_ = -velocity_tracker_.getVelocity();
            panning_ = false;
            arena_entry_.reset();
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            arena_entry_.reset();
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            break;

        case PointerEventKind::scroll:
        {
            const bool  is_v         = (axis == Axis::vertical);
            const float delta       = is_v ? event.scroll_delta_y : event.scroll_delta_x;
            const float cross_delta = is_v ? event.scroll_delta_x : event.scroll_delta_y;
            const bool  dominant    = std::abs(delta) >= std::abs(cross_delta);
            const bool  overscrolled = (raw_offset_ < min_extent_ || raw_offset_ > max_extent_);
            if (overscrolled && std::abs(delta) < kSignificantScrollDelta)
                break;
            if (auto* d = PointerDispatcher::activeDispatcher())
            {
                if (std::abs(delta) >= kSignificantScrollDelta)
                {
                    last_scroll_event_ms_ = currentMonotonicMs();
                    wheel_momentum_pending_ = true;
                }
                d->signalResolver().registerHandler([this, delta] { applyScrollDelta(delta, "wheel"); }, dominant);
            }
            break;
        }
        }
    }

    // -------------------------------------------------------------------------
    // Tick -- field-for-field mirrors RenderListView::onTick()
    // -------------------------------------------------------------------------

    void RenderViewport::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }

        if (now_ms - last_scroll_event_ms_ < kScrollActiveWindowMs)
        {
            last_tick_ms_ = now_ms;
            if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_ ||
                std::abs(velocity_px_s_) >= kMinVelocity)
                FrameScheduler::scheduleFrame();
            return;
        }

        if (wheel_momentum_pending_)
        {
            wheel_momentum_pending_ = false;
            if (physics->allowsMomentum())
                velocity_px_s_ = wheel_velocity_tracker_.getVelocity();
        }

        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        const float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        if (raw_offset_ < min_extent_ || raw_offset_ > max_extent_)
        {
            const float target = std::clamp(raw_offset_, min_extent_, max_extent_);
            const float eased_overscroll = (raw_offset_ - target) * std::exp(-kSpringCoeff * dt_s);
            const float new_raw = (std::abs(eased_overscroll) < kSpringSettleThreshold)
                ? target : (target + eased_overscroll);
            applyScrollDelta(new_raw - raw_offset_, "spring");
            velocity_px_s_ = 0.0f;
            FrameScheduler::scheduleFrame();
            return;
        }

        if (std::abs(velocity_px_s_) < kMinVelocity) { velocity_px_s_ = 0.0f; return; }

        applyScrollDelta(velocity_px_s_ * dt_s, "momentum");
        velocity_px_s_ = physics->applyFriction(velocity_px_s_, dt_s);
        if (std::abs(velocity_px_s_) >= kMinVelocity)
            FrameScheduler::scheduleFrame();
    }

} // namespace systems::leal::campello_widgets
