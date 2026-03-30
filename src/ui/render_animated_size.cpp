#include <campello_widgets/ui/render_animated_size.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/ticker.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    RenderAnimatedSize::RenderAnimatedSize(
        double                        duration_ms,
        std::function<double(double)> curve,
        Alignment                     alignment)
        : duration_ms_(duration_ms)
        , curve_(std::move(curve))
        , alignment_(alignment)
    {}

    RenderAnimatedSize::~RenderAnimatedSize()
    {
        stopAnimation();
    }

    void RenderAnimatedSize::setDuration(double ms) noexcept
    {
        duration_ms_ = ms;
    }

    void RenderAnimatedSize::setCurve(std::function<double(double)> fn) noexcept
    {
        curve_ = std::move(fn);
    }

    void RenderAnimatedSize::setAlignment(Alignment a) noexcept
    {
        if (!(a == alignment_)) {
            alignment_ = a;
            markNeedsPaint();
        }
    }

    // ------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------

    void RenderAnimatedSize::performLayout()
    {
        if (!child_) {
            size_ = constraints().constrain({0.0f, 0.0f});
            target_size_ = size_;
            return;
        }

        // Lay out child with loosened constraints to get its natural size.
        child_->layout(constraints().loosen());
        const Size child_size = child_->size();

        if (first_) {
            first_       = false;
            target_size_ = child_size;
            start_size_  = child_size;
            anim_t_      = 1.0;
        } else if (child_size.width  != target_size_.width ||
                   child_size.height != target_size_.height) {
            // Child size changed — start a new animation.
            start_size_  = currentSize();
            target_size_ = child_size;
            anim_t_      = 0.0;
            startAnimation();
        }

        size_ = constraints().constrain(currentSize());
    }

    // ------------------------------------------------------------------
    // Paint
    // ------------------------------------------------------------------

    void RenderAnimatedSize::performPaint(PaintContext& ctx, const Offset& offset)
    {
        if (!child_) return;

        // Clip to the animated bounds.
        ctx.canvas().clipRect(
            Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height));

        // Position child using alignment within the animated bounds.
        const Offset child_off = alignment_.inscribe(child_->size(), size_);
        paintChild(ctx, {offset.x + child_off.x, offset.y + child_off.y});
    }

    bool RenderAnimatedSize::hitTestChildren(
        HitTestResult& result, const Offset& position)
    {
        if (!child_) return false;
        const Offset child_off = alignment_.inscribe(child_->size(), size_);
        return child_->hitTest(result, {
            position.x - child_off.x,
            position.y - child_off.y
        });
    }

    // ------------------------------------------------------------------
    // Private helpers
    // ------------------------------------------------------------------

    Size RenderAnimatedSize::currentSize() const noexcept
    {
        if (anim_t_ >= 1.0) return target_size_;
        const double t = curve_ ? curve_(anim_t_) : anim_t_;
        return lerp<Size>(start_size_, target_size_, t);
    }

    void RenderAnimatedSize::startAnimation()
    {
        if (animating_) return;

        auto* ts = TickerScheduler::active();
        if (!ts) {
            anim_t_    = 1.0;
            return;
        }

        animating_    = true;
        last_tick_ms_ = 0;

        ticker_id_ = ts->subscribe([this](uint64_t now_ms) {
            if (last_tick_ms_ == 0) {
                last_tick_ms_ = now_ms;
                return;
            }

            const double dt    = static_cast<double>(now_ms - last_tick_ms_);
            last_tick_ms_      = now_ms;

            if (duration_ms_ > 0.0)
                anim_t_ = std::min(1.0, anim_t_ + dt / duration_ms_);
            else
                anim_t_ = 1.0;

            markNeedsLayout(); // triggers relayout which repaints

            if (anim_t_ >= 1.0)
                stopAnimation();
        });
    }

    void RenderAnimatedSize::stopAnimation()
    {
        if (!animating_) return;
        animating_ = false;

        auto* ts = TickerScheduler::active();
        if (ts && ticker_id_ != 0)
            ts->unsubscribe(ticker_id_);
        ticker_id_    = 0;
        last_tick_ms_ = 0;
    }

} // namespace systems::leal::campello_widgets
