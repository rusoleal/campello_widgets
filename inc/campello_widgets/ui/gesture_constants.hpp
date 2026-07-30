#pragma once

#include <campello_widgets/ui/pointer_event.hpp>

namespace systems::leal::campello_widgets
{

    // Mirrors Flutter's gestures/constants.dart. Touch contact is imprecise
    // (a finger covers many pixels), so touch-driven gestures need a larger
    // slop before movement is trusted as intentional; mouse/trackpad-cursor
    // input is pixel-precise and uses a much smaller one. Applying the touch
    // values uniformly (this engine's previous behavior) makes desktop
    // mouse/trackpad interaction feel noticeably less responsive than the
    // same widget in a real Flutter app.
    inline constexpr float kTouchSlop             = 18.0f;
    inline constexpr float kPanSlop               = kTouchSlop * 2.0f; // 36.0f
    inline constexpr float kPrecisePointerHitSlop = 1.0f;
    inline constexpr float kPrecisePointerPanSlop = 1.0f;

    /**
     * @brief Movement (px) past which a "hold still" gesture (tap,
     * double-tap, long-press) is cancelled — deliberately NOT device-aware.
     *
     * Unlike starting a drag, which should be razor-responsive for a precise
     * mouse, a stationary hold inherently involves a few pixels of drift
     * regardless of input device — a 500ms long-press held with a real mouse
     * will drift more than kPrecisePointerHitSlop's 1px. Flutter's
     * LongPressGestureRecognizer (via PrimaryPointerGestureRecognizer) does
     * not scale this tolerance down for precise pointers either; using the
     * device-aware hit slop here would make long-press practically
     * unreachable with a mouse.
     */
    inline constexpr float kStationaryTolerance = kTouchSlop;

    /**
     * @brief True for pointer kinds whose contact point is pixel-precise —
     * mouse/trackpad cursors and a stylus tip — as opposed to a finger,
     * which covers many pixels. Mirrors Flutter's own treatment of
     * `PointerDeviceKind.stylus`/`invertedStylus` alongside mouse/trackpad
     * in its gesture-slop logic (a pen tip is not a fingertip).
     */
    constexpr bool isPrecisePointer(PointerDeviceKind kind) noexcept
    {
        return kind == PointerDeviceKind::mouse ||
               kind == PointerDeviceKind::trackpad ||
               kind == PointerDeviceKind::stylus ||
               kind == PointerDeviceKind::invertedStylus;
    }

    /**
     * @brief Movement (px) past which a tap/press is no longer stationary.
     *
     * Used for gestures (e.g. Draggable) that mirror Flutter's
     * MultiDragGestureRecognizer, which resolves on hit slop rather than pan
     * slop. Not used for tap/long-press cancellation — see
     * kStationaryTolerance for why that needs a fixed, non-device-scaled
     * tolerance instead.
     */
    constexpr float computeHitSlop(PointerDeviceKind kind) noexcept
    {
        return isPrecisePointer(kind) ? kPrecisePointerHitSlop : kTouchSlop;
    }

    /**
     * @brief Movement (px) past which a gesture is recognized as a pan/drag.
     *
     * Used by scrollables and other PanGestureRecognizer-equivalent widgets.
     * Larger than hit slop for touch (2x) so that, e.g., a Draggable row
     * inside a scrollable list claims the gesture before the enclosing
     * scrollable does.
     */
    constexpr float computePanSlop(PointerDeviceKind kind) noexcept
    {
        return isPrecisePointer(kind) ? kPrecisePointerPanSlop : kPanSlop;
    }

} // namespace systems::leal::campello_widgets
