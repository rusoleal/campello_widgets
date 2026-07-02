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
     * @brief The kind of input device that produced a PointerEvent.
     *
     * Mirrors Flutter's `PointerDeviceKind`. Used to pick gesture slop
     * thresholds (see gesture_constants.hpp): touch contact is imprecise and
     * needs a larger slop, while mouse/trackpad-cursor input is precise and
     * uses a much smaller one.
     */
    enum class PointerDeviceKind : uint8_t
    {
        touch,
        mouse,
        stylus,
        invertedStylus,
        trackpad,
        unknown,
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
        PointerEventKind  kind           = PointerEventKind::move;
        int32_t           pointer_id     = 0;      ///< Finger/stylus ID; 0 for mouse
        Offset            position;                ///< Logical pixels from surface origin
        float             pressure       = 1.0f;   ///< 0.0–1.0; 1.0 for mouse, 0.0 for hover
        PointerDeviceKind device_kind    = PointerDeviceKind::touch; ///< Selects gesture slop
        float             scroll_delta_x = 0.0f;   ///< Scroll amount, x axis (scroll events only)
        float             scroll_delta_y = 0.0f;   ///< Scroll amount, y axis (scroll events only)

        /// Position in the recipient render box's own coordinate space, i.e.
        /// relative to its top-left corner. Filled in per-target by
        /// PointerDispatcher::dispatch() from the hit-test path, mirroring
        /// Flutter's `localPosition` on PointerEvent/DragDetails.
        Offset            local_position;
    };

} // namespace systems::leal::campello_widgets
