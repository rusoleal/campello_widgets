#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief RenderBox that clips and scrolls a single child along one axis.
     *
     * Layout: the child receives an unconstrained constraint on the scroll axis
     * so it can be as large as its content requires. The cross axis is tight to
     * the viewport size.
     *
     * Paint: the child is clipped to the viewport rectangle and translated by
     * the negative scroll offset so only the visible portion appears.
     *
     * Input: registers with the active PointerDispatcher to receive pan and
     * scroll-wheel events. Pan releases initiate momentum that is decayed each
     * tick by the active ScrollPhysics.
     */
    class RenderSingleChildScrollView : public RenderBox
    {
    public:
        Axis scroll_axis = Axis::vertical;

        RenderSingleChildScrollView();
        ~RenderSingleChildScrollView();

        void attach() override;
        void detach() override;

        /** @brief Wires an optional ScrollController for programmatic control. */
        void setController(std::shared_ptr<ScrollController> controller);

        /** @brief Replaces the active scroll physics (defaults to ClampingScrollPhysics). */
        void setPhysics(std::shared_ptr<ScrollPhysics> physics);

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        float scrollOffset() const noexcept;
        void  applyScrollDelta(float delta);

        std::shared_ptr<ScrollController> controller_;
        std::shared_ptr<ScrollPhysics>    physics_;

        // Internal offset used when no controller is attached.
        float internal_offset_ = 0.0f;

        // Scroll extents (updated after each layout).
        float min_extent_ = 0.0f;
        float max_extent_ = 0.0f;

        // Pan gesture state.
        bool   pointer_down_ = false;
        bool   panning_      = false;
        Offset pan_last_pos_;
        std::chrono::steady_clock::time_point last_pan_time_;
        float  pan_velocity_ = 0.0f; ///< Instantaneous velocity sampled during pan (px/s).

        // Momentum simulation.
        float    velocity_px_s_ = 0.0f;
        uint64_t last_tick_ms_  = 0;

        static constexpr float kTapSlop     = 8.0f;
        static constexpr float kMinVelocity = 1.0f;  ///< px/s below which momentum stops.
        static constexpr float kSpringCoeff = 12.0f; ///< Spring-back strength for bouncing physics.
    };

} // namespace systems::leal::campello_widgets
