#include <algorithm>
#include <cmath>
#include <limits>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
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

    void RenderSingleChildScrollView::applyScrollDelta(float delta)
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
    }

    // -------------------------------------------------------------------------
    // Input handling
    // -------------------------------------------------------------------------

    void RenderSingleChildScrollView::onPointerEvent(const PointerEvent& event)
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

            const bool  is_v  = (scroll_axis == Axis::vertical);
            const float dx    = event.position.x - pan_last_pos_.x;
            const float dy    = event.position.y - pan_last_pos_.y;

            if (!panning_ && std::sqrt(dx * dx + dy * dy) > kTapSlop)
                panning_ = true;

            if (panning_)
            {
                const float delta = is_v ? dy : dx;
                applyScrollDelta(-delta); // drag up → scroll down

                // Sample instantaneous velocity for momentum on release.
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

    void RenderSingleChildScrollView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        const float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        const float current = scrollOffset();

        // Spring back when overscrolled (BouncingScrollPhysics use case).
        if (current < min_extent_ || current > max_extent_)
        {
            const float target   = std::clamp(current, min_extent_, max_extent_);
            const float spring_v = (target - current) * kSpringCoeff;
            velocity_px_s_       = spring_v + velocity_px_s_ * 0.7f;
            applyScrollDelta(velocity_px_s_ * dt_s);
            return;
        }

        if (std::abs(velocity_px_s_) < kMinVelocity)
        {
            velocity_px_s_ = 0.0f;
            return;
        }

        applyScrollDelta(velocity_px_s_ * dt_s);
        velocity_px_s_ = physics_->applyFriction(velocity_px_s_, dt_s);
    }

} // namespace systems::leal::campello_widgets
