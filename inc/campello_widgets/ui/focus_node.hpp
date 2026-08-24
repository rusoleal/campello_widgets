#pragma once

#include <functional>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    class FocusManager;
    class RenderFocus;
    class RenderGestureDetector;

    /**
     * @brief Represents a focusable entity in the widget tree.
     *
     * Owned by a `Focus` widget (or created manually). Register it with a
     * `Focus` widget to participate in keyboard routing and tab traversal.
     *
     * Call `requestFocus()` to give this node keyboard focus.
     * Call `unfocus()` to release it. The `FocusManager` will call
     * `on_focus_changed` when the focus state changes.
     *
     * The `on_key` handler is called for every keyboard event while this
     * node has focus. Return true to mark the event as consumed (stops
     * further routing); return false to let it propagate.
     */
    class FocusNode
    {
    public:
        /** @brief Called when a key event arrives while this node is focused.
         *  Return true to consume the event, false to propagate. */
        std::function<bool(const KeyEvent&)> on_key;

        /** @brief Called with the new focus state whenever focus changes. */
        std::function<void(bool has_focus)> on_focus_changed;

        // ------------------------------------------------------------------
        // Focus control
        // ------------------------------------------------------------------

        /** @brief Requests keyboard focus from the active FocusManager. */
        void requestFocus();

        /** @brief Releases focus from this node. */
        void unfocus();

        /** @brief Returns true if this node currently has keyboard focus. */
        bool hasFocus() const noexcept { return focused_; }

        /**
         * @brief This node's on-screen bounds, in the window's global
         * (post-transform) coordinate space, as of the last paint.
         *
         * Updated every frame by whichever render object owns this node —
         * `RenderFocus` (nodes registered via the `Focus` widget) or
         * `RenderGestureDetector` (its own lazily-created/externally
         * supplied node when `focusable` is set — see its class doc).
         * Used by `FocusManager` for directional (arrow-key/D-pad)
         * navigation. `Rect::zero()` if this node hasn't been painted yet.
         */
        Rect bounds() const noexcept { return bounds_; }

    private:
        friend class FocusManager;
        friend class RenderFocus;
        friend class RenderGestureDetector;

        bool focused_ = false;
        Rect bounds_;
    };

} // namespace systems::leal::campello_widgets
