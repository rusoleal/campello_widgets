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

        /// False only for a HitTestBehavior::translucent claim -- tells an
        /// ancestor multi-child hit-test loop (e.g. RenderStack) that this
        /// entry doesn't block further, lower-painted siblings from also
        /// being hit-tested at the same point.
        bool       opaque         = true;
    };

    /**
     * @brief Controls how a RenderBox that claims pointer hits on its own
     * account (RenderGestureDetector today) interacts with its children's
     * own hit results and with lower-painted siblings in the same parent.
     *
     * Mirrors Flutter's HitTestBehavior.
     */
    enum class HitTestBehavior
    {
        /// Always claims the hit within its own bounds, regardless of
        /// whether a descendant also claimed it, and blocks further,
        /// lower-painted siblings (e.g. in a Stack) from being hit-tested.
        /// The default -- matches this codebase's behavior before
        /// HitTestBehavior existed.
        opaque,

        /// Always claims the hit like `opaque`, but does not block
        /// lower-painted siblings from also being hit-tested -- so e.g. a
        /// translucent overlay and the tappable content beneath it in a
        /// Stack can both receive the same tap.
        translucent,

        /// Only claims the hit when no descendant already did -- e.g. a
        /// row that should only react to taps landing outside its
        /// interactive children.
        deferToChild,
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
