#pragma once

#include <chrono>
#include <cstdint>
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

    /// Max gap (ms) between two qualifying taps for the second to count as
    /// a double-tap rather than two independent single taps.
    inline constexpr uint64_t kDoubleTapMs = 300;

    /// How long (ms) a pointer must be held stationary before it resolves
    /// as a long-press rather than a tap.
    inline constexpr uint64_t kLongPressMs = 500;

    /**
     * @brief True for pointer kinds whose contact point is pixel-precise —
     * a mouse cursor — as opposed to a finger or stylus tip.
     *
     * A stylus (and a finger) makes *physical contact* with the surface:
     * pressing down inherently pivots/rocks the tip a few pixels, the same
     * way a fingertip does, just less so — unlike a mouse cursor, which is
     * frictionlessly decoupled from the button click and can stay
     * genuinely stationary. Treating stylus as precise-pointer previously
     * gave it a 1px pan slop (kPrecisePointerPanSlop), which a real stylus
     * tap on glass exceeds almost every time — an ancestor scrollable
     * (SingleChildScrollView/ListView/...) would then win the gesture
     * arena over a button's tap on every single stylus tap, silently
     * swallowing it as an imperceptible scroll. Matches Flutter's actual
     * `computeHitSlop`/`computePanSlop` (gestures/constants.dart), which
     * only special-cases `PointerDeviceKind.mouse` this way — touch,
     * stylus, invertedStylus, and trackpad all fall through to the touch
     * slop there too.
     */
    constexpr bool isPrecisePointer(PointerDeviceKind kind) noexcept
    {
        return kind == PointerDeviceKind::mouse;
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

    /**
     * @brief Milliseconds since std::chrono::steady_clock's epoch, right now.
     *
     * Shared by gesture recognizers that stamp a pointer-down time (tap/
     * double-tap/long-press) — factored out so each recognizer doesn't
     * repeat the same duration_cast boilerplate.
     */
    inline uint64_t currentMonotonicMs() noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

} // namespace systems::leal::campello_widgets
