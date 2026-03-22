#pragma once

#include <vector>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    class RenderBox;

    /**
     * @brief Records a single render box that was struck by a hit-test.
     *
     * `local_position` is the hit point in the box's own coordinate space —
     * i.e. relative to the box's top-left corner, after all ancestor
     * translations have been removed.
     */
    struct HitTestEntry
    {
        RenderBox* target         = nullptr;
        Offset     local_position;
    };

    /**
     * @brief Accumulates HitTestEntry objects produced during a hit-test walk.
     *
     * Entries are appended deepest-first: the innermost box that was hit is
     * at index 0, and the root is at the back. This ordering matches gesture
     * recognizer dispatch (innermost gets first chance to handle the event).
     */
    class HitTestResult
    {
    public:
        /** @brief Appends an entry to the hit path. */
        void add(HitTestEntry entry) { path_.push_back(std::move(entry)); }

        /** @brief Returns the full hit path (deepest first). */
        const std::vector<HitTestEntry>& path() const noexcept { return path_; }

        /** @brief True if no render box was hit. */
        bool isEmpty() const noexcept { return path_.empty(); }

        /** @brief Removes all entries. */
        void clear() noexcept { path_.clear(); }

    private:
        std::vector<HitTestEntry> path_;
    };

} // namespace systems::leal::campello_widgets
