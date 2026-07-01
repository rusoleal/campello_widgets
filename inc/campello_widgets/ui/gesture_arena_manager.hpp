#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace systems::leal::campello_widgets
{

    /// Outcome a GestureArenaMember reports to the arena via GestureArenaEntry::resolve().
    enum class GestureDisposition
    {
        accepted,
        rejected,
    };

    class GestureArenaManager;

    /**
     * @brief Participates in gesture arbitration for a single pointer.
     *
     * Implemented by gesture recognizers (or the render objects that own
     * them). acceptGesture()/rejectGesture() are invoked by
     * GestureArenaManager once the arena resolves in this member's favour or
     * against it, respectively.
     */
    class GestureArenaMember
    {
    public:
        virtual ~GestureArenaMember() = default;

        /** @brief This member won the arena for `pointer_id`; continue tracking the gesture. */
        virtual void acceptGesture(int32_t pointer_id) = 0;

        /** @brief This member lost the arena for `pointer_id`; abandon the gesture. */
        virtual void rejectGesture(int32_t pointer_id) = 0;
    };

    /**
     * @brief A member's handle into an arena, returned by GestureArenaManager::add().
     *
     * Call resolve() at most once per entry to claim victory or concede defeat.
     */
    class GestureArenaEntry
    {
    public:
        /**
         * @brief Resolves this member's participation in the arena.
         *
         * `accepted` wins immediately if the arena is already closed; if the
         * arena is still open (more members may yet join), the win is
         * recorded as the eager winner and applied once close() runs, so
         * siblings that haven't registered yet aren't unfairly rejected.
         * `rejected` removes this member immediately; if that leaves exactly
         * one member in a closed arena, that member wins by default.
         */
        void resolve(GestureDisposition disposition);

    private:
        friend class GestureArenaManager;
        GestureArenaEntry(GestureArenaManager& manager, int32_t pointer_id, GestureArenaMember* member) noexcept
            : manager_(manager), pointer_id_(pointer_id), member_(member)
        {
        }

        GestureArenaManager& manager_;
        int32_t              pointer_id_;
        GestureArenaMember*  member_;
    };

    /**
     * @brief Arbitrates between competing GestureArenaMembers for a pointer.
     *
     * Mirrors Flutter's GestureArenaManager. A pointer-down event opens an
     * arena; recognizers add() themselves as they decide to track the
     * pointer. Once the down event has been dispatched to every handler in
     * the hit-test path, the dispatcher calls close() — after that the arena
     * resolves as soon as a member explicitly accepts, as soon as only one
     * member remains, or — failing either — when sweep() is called on
     * pointer up/cancel, in which case the first member added wins and
     * everyone else is rejected.
     *
     * There is at most one active manager at a time, set as a process-wide
     * singleton via setActive() (mirrors PointerDispatcher / DragManager).
     */
    class GestureArenaManager
    {
    public:
        GestureArenaManager()  = default;
        ~GestureArenaManager() = default;

        GestureArenaManager(const GestureArenaManager&)            = delete;
        GestureArenaManager& operator=(const GestureArenaManager&) = delete;

        /** @brief Joins the arena for `pointer_id`, creating it if it doesn't exist yet. */
        GestureArenaEntry add(int32_t pointer_id, GestureArenaMember* member);

        /**
         * @brief Marks the arena as closed (no more members will join).
         *
         * Called once the triggering pointer-down event has been dispatched
         * to every handler in the hit-test path.
         */
        void close(int32_t pointer_id);

        /**
         * @brief Forces resolution of a still-unresolved arena (pointer up/cancel).
         *
         * If nobody has explicitly resolved by now, the first member added
         * wins and the rest are rejected. No-op if the arena already resolved.
         */
        void sweep(int32_t pointer_id);

        /**
         * @brief Discards all in-flight arenas without firing any callbacks.
         *
         * Used when the underlying render tree is being torn down (e.g.
         * PointerDispatcher::setRoot()) and the members an arena refers to
         * may no longer be valid to call back into.
         */
        void reset() noexcept { arenas_.clear(); }

        /**
         * @brief Removes a member from every active arena without firing any
         * callbacks on it.  Called from the member's destructor or detach() so
         * that a dying render object is never called back into via sweep() or
         * resolve().
         */
        void removeMember(GestureArenaMember* member) noexcept;

        static void                 setActive(GestureArenaManager* manager) noexcept { s_instance_ = manager; }
        static GestureArenaManager* active() noexcept { return s_instance_; }

    private:
        friend class GestureArenaEntry;

        struct ArenaState
        {
            std::vector<GestureArenaMember*> members;
            bool                             is_open      = true;
            GestureArenaMember*              eager_winner = nullptr;
        };

        void resolve(int32_t pointer_id, GestureArenaMember* member, GestureDisposition disposition);
        void tryToResolveArena(int32_t pointer_id, ArenaState& state);
        void resolveInFavorOf(int32_t pointer_id, ArenaState& state, GestureArenaMember* member);

        std::unordered_map<int32_t, ArenaState> arenas_;

        inline static GestureArenaManager* s_instance_ = nullptr;
    };

} // namespace systems::leal::campello_widgets
