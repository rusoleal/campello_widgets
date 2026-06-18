#pragma once

#include <atomic>
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
         * Clears all active pointer state (captured pointers, hover path, etc.)
         * because the old hit-test tree is no longer valid.
         */
        void setRoot(std::shared_ptr<RenderBox> root) noexcept;

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

        /**
         * @brief Marks a pointer as captured by a specific render box.
         * When captured, only the capturing box receives events for this pointer.
         */
        void capturePointer(int32_t pointer_id, RenderBox* box);

        /**
         * @brief Releases a captured pointer.
         */
        void releasePointer(int32_t pointer_id);

        /**
         * @brief Returns true if the given pointer is captured by any box.
         */
        bool isPointerCaptured(int32_t pointer_id) const;

        /**
         * @brief Returns the box that captured the given pointer, or nullptr.
         */
        RenderBox* getCapturingBox(int32_t pointer_id) const;

        // ------------------------------------------------------------------
        // Debug: pointer position tracking
        // ------------------------------------------------------------------

        struct PointerPosition {
            float x = 0.0f;
            float y = 0.0f;
            uint64_t timestamp_ms = 0;
        };

        /** @brief Returns the last 16 pointer positions for debug visualization. */
        const std::vector<PointerPosition>& recentPointerPositions() const noexcept
        {
            return recent_positions_;
        }

    private:
        struct ActivePointer
        {
            std::vector<HitTestEntry> path;       ///< Captured hit path (deepest first); local_position is the value at down time
            Offset                    down_position; ///< Global position at the down event, for delta-adjusting local_position on move/up
        };

        /// Dispatches `event` to each target in `path`, setting `local_position`
        /// on the per-target copy from the corresponding HitTestEntry.
        void dispatch(const std::vector<HitTestEntry>& path, const PointerEvent& event);

        /// Returns true if |box| is a live RenderObject.
        static bool isBoxAlive(RenderBox* box) noexcept;

        std::shared_ptr<RenderBox>                       root_;
        std::unordered_map<int32_t, ActivePointer>       active_pointers_;
        std::unordered_map<RenderBox*, Handler>          handlers_;
        std::unordered_map<RenderBox*, TickHandler>      tick_handlers_;
        std::vector<RenderBox*>                          last_hover_path_;
        std::unordered_map<int32_t, RenderBox*>          captured_pointers_;  ///< pointer_id -> capturing box
        std::vector<PointerPosition>                     recent_positions_;

        static std::atomic<PointerDispatcher*> s_active_dispatcher_;
    };

} // namespace systems::leal::campello_widgets
