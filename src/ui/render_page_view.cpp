#include <algorithm>
#include <cmath>
#include <chrono>
#include <campello_widgets/ui/render_page_view.hpp>
#include <campello_widgets/widgets/page_view.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // PageController
    // =========================================================================

    void PageController::attach(RenderPageView* rv)
    {
        render_ = rv;
        if (rv) rv->jumpToPage(initial_page);
    }

    void PageController::detach()
    {
        render_ = nullptr;
    }

    void PageController::jumpToPage(int page)
    {
        if (render_) render_->jumpToPage(page);
    }

    int PageController::currentPage() const
    {
        return render_ ? render_->currentPage() : initial_page;
    }

    // =========================================================================
    // RenderPageView
    // =========================================================================

    RenderPageView::RenderPageView() = default;

    RenderPageView::~RenderPageView()
    {
        if (auto* d = PointerDispatcher::activeDispatcher()) {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (controller_) controller_->detach();
    }

    void RenderPageView::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher()) {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
    }

    void RenderPageView::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher()) {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    // ------------------------------------------------------------------
    // Child management
    // ------------------------------------------------------------------

    void RenderPageView::insertChild(std::shared_ptr<RenderBox> box, int index)
    {
        if (index >= static_cast<int>(children_.size()))
            children_.resize(index + 1);
        children_[index] = PageChild{std::move(box)};
    }

    void RenderPageView::clearChildren()
    {
        children_.clear();
    }

    // ------------------------------------------------------------------
    // Controller
    // ------------------------------------------------------------------

    void RenderPageView::setController(std::shared_ptr<PageController> ctrl)
    {
        if (controller_) controller_->detach();
        controller_ = std::move(ctrl);
        if (controller_) controller_->attach(this);
    }

    void RenderPageView::jumpToPage(int page)
    {
        const float max_p = maxPageOffset();
        page_offset_ = std::clamp(static_cast<float>(page), 0.0f, max_p);
        markNeedsPaint();
    }

    // ------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------

    void RenderPageView::performLayout()
    {
        size_ = constraints_.constrain(
            Size{constraints_.max_width, constraints_.max_height});

        const bool  is_h = (scroll_direction == Axis::horizontal);
        const float vw   = size_.width;
        const float vh   = size_.height;
        const BoxConstraints page_cc = BoxConstraints::tight(vw, vh);

        for (auto& entry : children_) {
            if (entry.box) {
                layoutChild(*entry.box, page_cc);
            }
        }

        // Clamp page offset in case children were removed
        page_offset_ = std::clamp(page_offset_, 0.0f, maxPageOffset());
        (void)is_h;
    }

    // ------------------------------------------------------------------
    // Paint
    // ------------------------------------------------------------------

    void RenderPageView::performPaint(PaintContext& context, const Offset& offset)
    {
        if (children_.empty()) return;

        const bool  is_h   = (scroll_direction == Axis::horizontal);
        const float vw     = size_.width;
        const float vh     = size_.height;
        const float scroll = page_offset_ * (is_h ? vw : vh);

        auto& canvas = context.canvas();
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(offset.x, offset.y, vw, vh));

        for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
            auto& entry = children_[i];
            if (!entry.box) continue;

            const float page_pos = static_cast<float>(i) * (is_h ? vw : vh) - scroll;

            // Only paint pages that are at least partially visible
            if (is_h) {
                if (page_pos + vw < 0.0f || page_pos > vw) continue;
                entry.box->paint(context,
                    Offset{offset.x + page_pos, offset.y});
            } else {
                if (page_pos + vh < 0.0f || page_pos > vh) continue;
                entry.box->paint(context,
                    Offset{offset.x, offset.y + page_pos});
            }
        }

        canvas.restore();
    }

    // ------------------------------------------------------------------
    // Hit testing
    // ------------------------------------------------------------------

    bool RenderPageView::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        if (children_.empty()) return false;

        const bool  is_h   = (scroll_direction == Axis::horizontal);
        const float vw     = size_.width;
        const float vh     = size_.height;
        const float scroll = page_offset_ * (is_h ? vw : vh);

        for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
            auto& entry = children_[i];
            if (!entry.box) continue;

            const float page_pos = static_cast<float>(i) * (is_h ? vw : vh) - scroll;
            const Offset adjusted = is_h
                ? Offset{position.x - page_pos, position.y}
                : Offset{position.x, position.y - page_pos};

            if (entry.box->hitTest(result, adjusted)) return true;
        }
        return false;
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    float RenderPageView::viewportExtent() const noexcept
    {
        return scroll_direction == Axis::horizontal ? size_.width : size_.height;
    }

    float RenderPageView::maxPageOffset() const noexcept
    {
        const int n = static_cast<int>(children_.size());
        return static_cast<float>(std::max(0, n - 1));
    }

    void RenderPageView::applyPageDelta(float delta)
    {
        const float extent = viewportExtent();
        if (extent <= 0.0f) return;

        const float page_delta = delta / extent;
        page_offset_ = std::clamp(page_offset_ + page_delta, 0.0f, maxPageOffset());
        markNeedsPaint();
    }

    void RenderPageView::snapToPage()
    {
        const int n   = static_cast<int>(children_.size());
        if (n == 0) return;

        int target;
        if (std::abs(velocity_px_s_) > kSnapVelocityThreshold) {
            // Flick: go to next/previous page
            target = velocity_px_s_ > 0.0f
                ? static_cast<int>(std::ceil(page_offset_))
                : static_cast<int>(std::floor(page_offset_));
        } else {
            target = static_cast<int>(std::round(page_offset_));
        }

        target = std::clamp(target, 0, n - 1);
        page_offset_   = static_cast<float>(target);
        velocity_px_s_ = 0.0f;
        markNeedsPaint();

        if (target != last_page_) {
            last_page_ = target;
            if (on_page_changed) on_page_changed(target);
        }
    }

    // ------------------------------------------------------------------
    // Input
    // ------------------------------------------------------------------

    void RenderPageView::onPointerEvent(const PointerEvent& event)
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

            const float dx = event.position.x - pan_last_pos_.x;
            const float dy = event.position.y - pan_last_pos_.y;

            if (!panning_ && std::sqrt(dx * dx + dy * dy) > kTapSlop)
                panning_ = true;

            if (panning_) {
                const bool  is_h  = (scroll_direction == Axis::horizontal);
                const float delta = is_h ? dx : dy;
                applyPageDelta(-delta);

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
            if (panning_) {
                velocity_px_s_ = pan_velocity_;
                snapToPage();
            }
            panning_ = false;
            break;

        case PointerEventKind::cancel:
            pointer_down_ = false;
            panning_ = false;
            break;

        default:
            break;
        }
    }

    void RenderPageView::onTick(uint64_t now_ms)
    {
        if (panning_) { last_tick_ms_ = now_ms; return; }
        if (last_tick_ms_ == 0) { last_tick_ms_ = now_ms; return; }

        const float dt_s = static_cast<float>(now_ms - last_tick_ms_) / 1000.0f;
        last_tick_ms_ = now_ms;

        if (std::abs(velocity_px_s_) < kMinVelocity) {
            velocity_px_s_ = 0.0f;
            return;
        }

        applyPageDelta(velocity_px_s_ * dt_s);
        velocity_px_s_ *= std::exp(-kSpringCoeff * dt_s);
    }


    void RenderPageView::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& c : children_)
            if (c.box.get()) visitor(c.box.get());
    }
} // namespace systems::leal::campello_widgets
