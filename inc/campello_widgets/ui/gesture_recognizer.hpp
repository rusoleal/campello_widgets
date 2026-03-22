#pragma once

#include <cstdint>
#include <campello_widgets/ui/pointer_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Abstract base for all gesture recognizers.
     *
     * A GestureRecognizer receives a stream of pointer events and optional
     * per-frame tick callbacks, then fires user-supplied callbacks when it
     * detects its gesture pattern.
     *
     * Events are fed by the owning render object (e.g. RenderGestureDetector)
     * rather than self-registered, because PointerDispatcher maps one handler
     * per RenderBox and a single box may host several recognizers.
     *
     * Concrete recognizers override handlePointerEvent() and, where timing is
     * needed (e.g. long press), handleTick(). They invoke their own callbacks
     * internally when the gesture is recognised.
     *
     * Ownership: recognizers are created and held by the widget or render
     * object that configures the gestures. The owner must call dispose()
     * before destroying the recognizer.
     */
    class GestureRecognizer
    {
    public:
        virtual ~GestureRecognizer() = default;

        /**
         * @brief Feed a pointer event into the recognizer.
         *
         * Called by the owning render object for every pointer event that
         * falls within its bounds or is part of an active pointer sequence.
         */
        virtual void handlePointerEvent(const PointerEvent& event) = 0;

        /**
         * @brief Called once per frame with the current monotonic timestamp.
         *
         * Only needed by time-based recognizers (e.g. long press).
         * The default implementation is a no-op.
         *
         * @param now_ms Milliseconds from std::chrono::steady_clock epoch.
         */
        virtual void handleTick(uint64_t /*now_ms*/) {}

        /**
         * @brief Release any resources held by the recognizer.
         *
         * Called by the owner before the recognizer is destroyed.
         * The default implementation is a no-op; subclasses may override.
         */
        virtual void dispose() {}

    protected:
        GestureRecognizer() = default;
    };

} // namespace systems::leal::campello_widgets
