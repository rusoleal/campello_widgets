#pragma once

#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <functional>
#include <typeindex>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Global drag-and-drop session manager.
     *
     * DragManager tracks the in-progress drag, registered DragTarget render
     * objects, and dispatches enter/exit/accept callbacks as the pointer moves.
     *
     * Draggable<T> calls startSession() / updatePosition() / endSession().
     * RenderDragTarget registers with registerTarget() and updates its bounds
     * via updateTargetBounds() each paint cycle.
     *
     * There is at most one active drag at a time. DragManager is set as a
     * process-wide singleton via setActive().
     */
    class DragManager
    {
    public:
        DragManager()  = default;
        ~DragManager() = default;

        DragManager(const DragManager&)            = delete;
        DragManager& operator=(const DragManager&) = delete;

        // ------------------------------------------------------------------
        // Session control (called by Draggable internals)
        // ------------------------------------------------------------------

        /**
         * @brief Begins a drag session.
         *
         * @param type  Runtime type of the dragged data (typeid(T)).
         * @param data  Pointer to the drag data (owned by the caller for the
         *              session's lifetime).
         * @param pos   Initial pointer position in global coordinates.
         */
        void startSession(std::type_index type, const void* data, Offset pos);

        /**
         * @brief Updates the pointer position and evaluates hover over targets.
         */
        void updatePosition(Offset pos);

        /**
         * @brief Ends the session, firing on_accept on the topmost accepting target.
         * @return true if any target accepted the drop.
         */
        bool endSession();

        /** @brief Returns true while a drag is in progress. */
        bool isDragging() const noexcept { return dragging_; }

        /** @brief Type of the data being dragged (valid while isDragging()). */
        std::type_index sessionType() const noexcept { return current_type_; }

        /** @brief Pointer to the drag data (valid while isDragging()). */
        const void* sessionData() const noexcept { return current_data_; }

        /** @brief Current drag pointer position. */
        Offset sessionPosition() const noexcept { return current_pos_; }

        // ------------------------------------------------------------------
        // Target registration (called by RenderDragTarget)
        // ------------------------------------------------------------------

        /**
         * @brief Registers a DragTarget render object.
         *
         * @param id         Unique identity (pointer to the render object).
         * @param will_accept Returns true if the current session data is acceptable.
         * @param on_enter   Called when the drag pointer enters the target's bounds.
         * @param on_exit    Called when the drag pointer leaves the target's bounds.
         * @param on_accept  Called when the drag is dropped on this target.
         */
        void registerTarget(void*                  id,
                            std::function<bool()>  will_accept,
                            std::function<void()>  on_enter,
                            std::function<void()>  on_exit,
                            std::function<void()>  on_accept);

        /** @brief Unregisters a previously registered target. */
        void unregisterTarget(void* id);

        /**
         * @brief Updates the target's global bounding rect (call each paint).
         */
        void updateTargetBounds(void* id, const Rect& global_bounds);

        // ------------------------------------------------------------------
        // Global accessor
        // ------------------------------------------------------------------

        static void         setActive(DragManager* m) noexcept { s_instance_ = m; }
        static DragManager* active() noexcept                  { return s_instance_; }

    private:
        struct TargetRecord
        {
            void*                  id;
            Rect                   bounds;
            std::function<bool()>  will_accept;
            std::function<void()>  on_enter;
            std::function<void()>  on_exit;
            std::function<void()>  on_accept;
            bool                   hovered = false;
        };

        void checkHovers(Offset pos);
        TargetRecord* findTarget(void* id);

        bool            dragging_     = false;
        std::type_index current_type_ { typeid(void) };
        const void*     current_data_ = nullptr;
        Offset          current_pos_;

        std::vector<TargetRecord> targets_;

        static DragManager* s_instance_;
    };

} // namespace systems::leal::campello_widgets
