#pragma once

#include <cstdint>
#include <optional>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Abstract base for all gesture recognizers.
     *
     * A GestureRecognizer is itself a GestureArenaMember: it joins the
     * gesture arena directly for every pointer it wants to track, competing
     * both against other recognizers on the same RenderGestureDetector (a
     * TapGestureRecognizer and a PanGestureRecognizer on the same detector
     * are two independent members of the same arena) and against ancestor/
     * descendant recognizers (e.g. an enclosing scrollable). This mirrors
     * Flutter's real GestureDetector, where each configured gesture family
     * gets its own recognizer rather than one widget hard-coding the
     * arbitration between all of them.
     *
     * Events are fed by the owning render object (e.g. RenderGestureDetector)
     * rather than self-registered with PointerDispatcher, because the
     * dispatcher maps one handler per RenderBox and a single box may host
     * several recognizers.
     *
     * Concrete recognizers override addPointer()/handlePointerEvent() and,
     * where timing is needed (e.g. long press), handleTick(). They invoke
     * their own callbacks internally when their gesture is recognised.
     *
     * Ownership: recognizers are created and held by the render object that
     * configures the gestures. The owner must call dispose() before
     * destroying the recognizer.
     */
    class GestureRecognizer : public GestureArenaMember
    {
    public:
        ~GestureRecognizer() override = default;

        /**
         * @brief A new pointer sequence has started within this recognizer's
         * bounds. Join the gesture arena for it here (see gesture_arena_manager.hpp)
         * if this recognizer wants a say in how the sequence resolves.
         */
        virtual void addPointer(const PointerEvent& down) = 0;

        /**
         * @brief Feed a pointer event (move/up/cancel/scroll) into the
         * recognizer, for a pointer previously passed to addPointer().
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
         * @brief Release any resources held by the recognizer, including
         * any still-open arena entry. Called by the owner before the
         * recognizer is destroyed, and by the owner's detach().
         *
         * The default implementation removes this recognizer from every
         * arena it may still be a member of (safe even if it never joined
         * one, or already resolved). Subclasses with extra state to clear
         * should call the base implementation too.
         */
        virtual void dispose();

    protected:
        GestureRecognizer() = default;
    };

} // namespace systems::leal::campello_widgets
