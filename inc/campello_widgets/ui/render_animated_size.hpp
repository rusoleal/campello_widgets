#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <cstdint>
#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderObject that animates its own size when the child's
     * intrinsic size changes.
     *
     * On the first layout the RenderObject snaps to the child's size.
     * On subsequent layouts, if the child reports a different size, the
     * render object smoothly interpolates from the current size to the new
     * one via a TickerScheduler subscription.
     */
    class RenderAnimatedSize final : public RenderBox
    {
    public:
        RenderAnimatedSize(
            double                        duration_ms,
            std::function<double(double)> curve,
            Alignment                     alignment);

        ~RenderAnimatedSize() override;

        void setDuration(double ms)                      noexcept;
        void setCurve(std::function<double(double)> fn)  noexcept;
        void setAlignment(Alignment a)                   noexcept;

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout()                                                override;
        void performPaint(PaintContext& ctx, const Offset& offset)          override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        double                        duration_ms_;
        std::function<double(double)> curve_;
        Alignment                     alignment_;

        Size     target_size_{};  ///< Child's desired size
        Size     start_size_{};   ///< Size at animation start
        double   anim_t_     = 1.0; ///< 0 = start_size_, 1 = target_size_
        bool     first_      = true; ///< Snap on first layout

        bool     animating_      = false;
        uint64_t ticker_id_      = 0;
        uint64_t last_tick_ms_   = 0;

        Size currentSize() const noexcept;
        void startAnimation();
        void stopAnimation();
    };

} // namespace systems::leal::campello_widgets
