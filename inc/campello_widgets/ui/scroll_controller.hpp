#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include <campello_widgets/ui/animation_controller.hpp>

namespace systems::leal::campello_widgets
{

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

        /**
         * @brief Registers a callback that fires with the raw (unresisted)
         * top-edge overscroll distance, in logical pixels.
         *
         * Positive = pulled past the top boundary by that many px, before
         * any boundary-resistance damping (ClampingScrollPhysics clamps the
         * *displayed* offset() at the boundary while this keeps advancing;
         * BouncingScrollPhysics rubber-bands the displayed offset by less
         * than this). Zero = not overscrolled at the top, whether in-bounds
         * or overscrolled at the *bottom* — this channel only ever reports
         * the top edge, matching RefreshIndicator's one use for it.
         *
         * Separate from addListener() because that one only ever carries
         * the boundary-resisted offset() — this is the only way to observe
         * how far past the edge a drag actually went.
         *
         * @return A listener ID for use with removeOverscrollListener().
         */
        uint64_t addOverscrollListener(std::function<void(float)> fn);

        /** @brief Removes the overscroll listener with the given ID. */
        void removeOverscrollListener(uint64_t id);

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

        /** @brief Called by the render object on every scroll-delta application. */
        void notifyOverscroll(float raw_overscroll);

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

        uint64_t next_overscroll_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void(float)>>> overscroll_listeners_;
    };

} // namespace systems::leal::campello_widgets
