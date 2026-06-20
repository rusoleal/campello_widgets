#pragma once

#include <functional>
#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that draws a track + thumb slider and handles pointer
     *        events to compute a normalised value [0, 1].
     *
     * Registered with PointerDispatcher on construction. Tracks its own global
     * origin from `performPaint` to convert pointer positions to local coords.
     */
    class RenderSlider : public RenderBox, public GestureArenaMember
    {
    public:
        float  value          = 0.0f;     ///< Normalised [0, 1]
        Color  active_color   = Color::fromRGBA(0.051f, 0.545f, 0.553f, 1.0f);
        Color  inactive_color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.26f);
        float  track_height   = 4.0f;
        float  thumb_radius   = 10.0f;

        /** Called with normalised value [0, 1] on pointer down and move. */
        std::function<void(float)> on_value_changed;

        RenderSlider();
        ~RenderSlider() override;

        void attach() override;
        void detach() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        // ------------------------------------------------------------------
        // GestureArenaMember
        // ------------------------------------------------------------------

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        void  onPointerEvent(const PointerEvent& event);
        float positionToValue(float local_x) const noexcept;

        bool   pressed_       = false;
        bool   won_arena_     = false;
        bool   lost_arena_    = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset global_offset_;           ///< Latched each paint to convert pointer coords
    };

} // namespace systems::leal::campello_widgets
