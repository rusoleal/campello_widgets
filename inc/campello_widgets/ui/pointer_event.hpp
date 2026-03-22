#pragma once

#include <cstdint>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief The phase of a pointer contact event.
     *
     *  down    — pointer touched / mouse button pressed
     *  move    — pointer moved while down, or hover move while up
     *  up      — pointer lifted / mouse button released
     *  cancel  — system cancelled the pointer sequence (e.g. app lost focus)
     *  scroll  — scroll-wheel or trackpad scroll; use scroll_delta_x/y
     */
    enum class PointerEventKind : uint8_t
    {
        down,
        move,
        up,
        cancel,
        scroll,
    };

    /**
     * @brief A single platform-agnostic pointer contact or scroll event.
     *
     * Produced by platform adapters (macOS, iOS, Android, …) and fed into
     * PointerDispatcher for routing through the widget tree.
     *
     * Coordinate system: origin at the top-left of the widget surface,
     * x increasing right, y increasing down, in logical pixels.
     *
     * For scroll events: `position` is the cursor location; `scroll_delta_x`
     * and `scroll_delta_y` hold the scroll amounts (positive = scroll right/down).
     */
    struct PointerEvent
    {
        PointerEventKind kind          = PointerEventKind::move;
        int32_t          pointer_id   = 0;      ///< Finger/stylus ID; 0 for mouse
        Offset           position;              ///< Logical pixels from surface origin
        float            pressure     = 1.0f;  ///< 0.0–1.0; 1.0 for mouse, 0.0 for hover
        float            scroll_delta_x = 0.0f; ///< Scroll amount, x axis (scroll events only)
        float            scroll_delta_y = 0.0f; ///< Scroll amount, y axis (scroll events only)
    };

} // namespace systems::leal::campello_widgets
