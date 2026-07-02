#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>
#include <campello_widgets/ui/offset_layer.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace systems::leal::campello_widgets
{

    class PageController;

    /**
     * @brief RenderBox that lays out children as full-viewport pages and
     *        handles horizontal (or vertical) swipe-to-page gestures.
     *
     * Each child receives tight constraints equal to the viewport size.
     * Only adjacent pages are painted. Swipe gestures snap to the nearest
     * integer page index.
     *
     * Self-boundaries its own paint output via `OffsetLayer` — see
     * `RenderSingleChildScrollView`'s class doc for the rationale, which
     * applies identically here.
     */
    class RenderPageView : public RenderBox, public GestureArenaMember
    {
    public:
        Axis                      scroll_direction = Axis::horizontal;
        std::function<void(int)>  on_page_changed;

        RenderPageView();
        ~RenderPageView();

        void attach() override;
        void detach() override;

        // ------------------------------------------------------------------
        // Child management — called by MultiChildRenderObjectElement
        // ------------------------------------------------------------------

        void insertChild(std::shared_ptr<RenderBox> box, int index);
        void clearChildren();

        // ------------------------------------------------------------------
        // Controller
        // ------------------------------------------------------------------

        void setController(std::shared_ptr<PageController> ctrl);

        /** @brief Jumps to the given page immediately (no animation). */
        void jumpToPage(int page);

        /** @brief Returns the current page index. */
        int currentPage() const noexcept { return static_cast<int>(page_offset_ + 0.5f); }

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return true; }
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

        // Must claim hits within its own viewport to receive pan-to-swipe
        // gestures — see RenderBox::hitTestSelf().
        bool hitTestSelf(const Offset&) const override { return true; }

        // ------------------------------------------------------------------
        // GestureArenaMember
        // ------------------------------------------------------------------

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        float viewportExtent() const noexcept;
        float maxPageOffset()  const noexcept;
        void  applyPageDelta(float delta);
        void  snapToPage();

        struct PageChild
        {
            std::shared_ptr<RenderBox> box;
        };
        std::vector<PageChild> children_;
        OffsetLayer            offset_layer_;

        std::shared_ptr<PageController> controller_;

        float    page_offset_    = 0.0f;   ///< Fractional page position.
        int      last_page_      = 0;

        bool     pointer_down_   = false;
        bool     panning_        = false;
        bool     won_arena_      = false;
        bool     lost_arena_     = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset   pan_last_pos_;
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;
        std::chrono::steady_clock::time_point last_pan_time_;
        float    pan_velocity_   = 0.0f;

        float    velocity_px_s_  = 0.0f;
        uint64_t last_tick_ms_   = 0;

        static constexpr float kSnapVelocityThreshold = 300.0f;
        static constexpr float kSpringCoeff           = 20.0f;
        static constexpr float kMinVelocity           = 0.5f;
    };

} // namespace systems::leal::campello_widgets
