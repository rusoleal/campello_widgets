#pragma once

#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that draws a track + thumb slider and handles pointer
     *        events to compute a normalised value [0, 1].
     *
     * Registered with PointerDispatcher on construction. Tracks its own global
     * origin from `performPaint` to convert pointer positions to local coords.
     */
    class RenderSlider : public RenderBox
    {
    public:
        float  value          = 0.0f;     ///< Normalised [0, 1]
        Color  active_color   = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
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

    private:
        void  onPointerEvent(const PointerEvent& event);
        float positionToValue(float local_x) const noexcept;

        bool   pressed_       = false;
        Offset global_offset_;           ///< Latched each paint to convert pointer coords
    };

} // namespace systems::leal::campello_widgets
