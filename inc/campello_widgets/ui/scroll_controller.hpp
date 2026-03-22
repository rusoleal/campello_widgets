#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace systems::leal::campello_widgets
{

    class AnimationController;

    /**
     * @brief Controls the scroll offset of a scrollable widget.
     *
     * A ScrollController is created by the user and passed to a scrollable
     * widget (SingleChildScrollView, ListView, GridView). The render object
     * calls attach() / detach() when it mounts / unmounts, and setExtents()
     * after each layout pass so the controller can clamp positions.
     *
     * Typical use:
     * @code
     * auto ctrl = std::make_shared<ScrollController>();
     * ctrl->addListener([&] { setState([]{}); });
     * // ... pass ctrl to SingleChildScrollView ...
     * ctrl->animateTo(200.0f);
     * @endcode
     */
    class ScrollController
    {
    public:
        ScrollController() = default;
        ~ScrollController();

        // Non-copyable — owns animation state.
        ScrollController(const ScrollController&)            = delete;
        ScrollController& operator=(const ScrollController&) = delete;

        // ------------------------------------------------------------------
        // Position
        // ------------------------------------------------------------------

        /** @brief Current scroll offset in logical pixels. */
        float offset() const noexcept { return offset_; }

        /** @brief Minimum valid scroll offset (usually 0). */
        float minScrollExtent() const noexcept { return min_extent_; }

        /** @brief Maximum valid scroll offset (content size − viewport size). */
        float maxScrollExtent() const noexcept { return max_extent_; }

        /** @brief True while an attached scrollable render object exists. */
        bool hasClients() const noexcept { return attached_; }

        // ------------------------------------------------------------------
        // Control
        // ------------------------------------------------------------------

        /**
         * @brief Instantly jumps to the given offset (clamped to valid range).
         */
        void jumpTo(float offset);

        /**
         * @brief Smoothly animates to the given offset.
         *
         * @param offset      Target offset (clamped to valid range).
         * @param duration_ms Animation duration in milliseconds (default 300 ms).
         */
        void animateTo(float offset, double duration_ms = 300.0);

        // ------------------------------------------------------------------
        // Listener API
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback that fires whenever the offset changes.
         * @return A listener ID for use with removeListener().
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Removes the listener with the given ID. */
        void removeListener(uint64_t id);

        // ------------------------------------------------------------------
        // Called by the render object
        // ------------------------------------------------------------------

        /**
         * @brief Reports current scroll extents so the controller can clamp positions.
         *
         * Called by the attached render object after each layout pass.
         */
        void setExtents(float min_extent, float max_extent) noexcept;

        /** @brief Called when a scrollable render object mounts. */
        void attach() noexcept { attached_ = true; }

        /** @brief Called when the scrollable render object unmounts. */
        void detach() noexcept { attached_ = false; }

    private:
        void setOffset(float offset);
        void notifyListeners();

        float offset_     = 0.0f;
        float min_extent_ = 0.0f;
        float max_extent_ = 0.0f;
        bool  attached_   = false;

        std::unique_ptr<AnimationController> anim_;
        uint64_t anim_listener_id_ = 0;

        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;
    };

} // namespace systems::leal::campello_widgets
