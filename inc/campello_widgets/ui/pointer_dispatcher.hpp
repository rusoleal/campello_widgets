#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/hit_test.hpp>

namespace systems::leal::campello_widgets
{

    class RenderBox;

    /**
     * @brief Routes pointer events through the render tree via hit-testing.
     *
     * Usage:
     *  1. Construct with the root RenderBox.
     *  2. Call `addHandler()` from any render object that wants to receive events
     *     (GestureDetector's render object will do this).
     *  3. Feed platform events via `handlePointerEvent()`.
     *
     * Pointer capture semantics:
     *  - On `down`: hit-tests the tree, caches the hit path for this pointer_id,
     *    then dispatches to all registered handlers in the path (deepest first).
     *  - On `move` with an active capture: dispatches to the cached path.
     *  - On `move` without an active capture (hover): re-hit-tests each call.
     *  - On `up` / `cancel`: dispatches to the cached path, then clears it.
     */
    class PointerDispatcher
    {
    public:
        using Handler     = std::function<void(const PointerEvent&)>;
        using TickHandler = std::function<void(uint64_t now_ms)>;

        explicit PointerDispatcher(std::shared_ptr<RenderBox> root = nullptr);

        /**
         * @brief Sets (or replaces) the root render box used for hit-testing.
         *
         * Call after the widget tree is mounted and the root RenderBox is known.
         */
        void setRoot(std::shared_ptr<RenderBox> root) noexcept { root_ = std::move(root); }

        // ------------------------------------------------------------------
        // Handler registration
        // ------------------------------------------------------------------

        /**
         * @brief Registers a handler called whenever `box` is in the hit path.
         *
         * Overwrites any previously registered handler for the same box.
         * Called by GestureDetector's render object on mount/update.
         */
        void addHandler(RenderBox* box, Handler handler);

        /** @brief Removes the handler for `box`, if any. */
        void removeHandler(RenderBox* box);

        // ------------------------------------------------------------------
        // Tick registration (for time-based recognizers, e.g. long press)
        // ------------------------------------------------------------------

        /**
         * @brief Registers a per-frame tick callback for `box`.
         *
         * Called by gesture recognizers that need time-based logic (long press).
         * `now_ms` is milliseconds from std::chrono::steady_clock epoch.
         */
        void addTickHandler(RenderBox* box, TickHandler handler);

        /** @brief Removes the tick handler for `box`, if any. */
        void removeTickHandler(RenderBox* box);

        /**
         * @brief Fires all registered tick handlers.
         *
         * Called once per frame from Renderer::renderFrame.
         *
         * @param now_ms Milliseconds from steady_clock epoch.
         */
        void tick(uint64_t now_ms);

        // ------------------------------------------------------------------
        // Event entry point
        // ------------------------------------------------------------------

        /**
         * @brief Routes a pointer event to any registered handlers in the hit path.
         *
         * Must be called from the render thread (same thread as renderFrame).
         * Scroll events (kind == scroll) are re-hit-tested each call; they do
         * not use pointer capture.
         */
        void handlePointerEvent(const PointerEvent& event);

        // ------------------------------------------------------------------
        // Global accessor (set by the platform adapter, used by render objects)
        // ------------------------------------------------------------------

        /**
         * @brief Sets the dispatcher available during the current session.
         *
         * Called by the platform adapter (e.g. runApp on macOS) after the
         * dispatcher is created. Render objects call `activeDispatcher()` in
         * their constructor to register gesture handlers.
         */
        static void setActiveDispatcher(PointerDispatcher* dispatcher) noexcept;

        /** @brief Returns the dispatcher set for the current session, or nullptr. */
        static PointerDispatcher* activeDispatcher() noexcept;

    private:
        struct ActivePointer
        {
            std::vector<RenderBox*> path; ///< Captured hit path (deepest first)
        };

        void dispatch(const std::vector<RenderBox*>& path, const PointerEvent& event);

        std::shared_ptr<RenderBox>                       root_;
        std::unordered_map<int32_t, ActivePointer>       active_pointers_;
        std::unordered_map<RenderBox*, Handler>          handlers_;
        std::unordered_map<RenderBox*, TickHandler>      tick_handlers_;
        std::vector<RenderBox*>                          last_hover_path_;

        static PointerDispatcher* s_active_dispatcher_;
    };

} // namespace systems::leal::campello_widgets
