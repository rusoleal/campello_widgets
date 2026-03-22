#pragma once

#include <vector>
#include <campello_widgets/ui/key_event.hpp>

namespace systems::leal::campello_widgets
{

    class FocusNode;

    /**
     * @brief Routes keyboard events to the currently focused FocusNode and
     *        manages tab-order focus traversal.
     *
     * Usage:
     *  - The platform adapter creates a FocusManager and calls
     *    `setActiveManager()` before mounting the widget tree.
     *  - `Focus` render objects call `registerNode()`/`unregisterNode()` in
     *    their constructor/destructor.
     *  - The platform adapter calls `handleKeyEvent()` on each key press.
     *
     * Tab traversal:
     *  - Tab moves focus forward through registered nodes (in registration order).
     *  - Shift+Tab moves backward.
     *  - Both wrap around.
     */
    class FocusManager
    {
    public:
        FocusManager() = default;

        // ------------------------------------------------------------------
        // Node registration
        // ------------------------------------------------------------------

        /** @brief Registers a node for focus routing and tab traversal. */
        void registerNode(FocusNode* node);

        /** @brief Unregisters a node; if it was focused, focus is cleared. */
        void unregisterNode(FocusNode* node);

        // ------------------------------------------------------------------
        // Focus control
        // ------------------------------------------------------------------

        /** @brief Gives keyboard focus to `node`. */
        void requestFocus(FocusNode* node);

        /** @brief Clears focus from `node` (no-op if `node` is not focused). */
        void unfocus(FocusNode* node);

        // ------------------------------------------------------------------
        // Key event routing
        // ------------------------------------------------------------------

        /**
         * @brief Routes a keyboard event to the focused node.
         *
         * Tab / Shift+Tab are intercepted for focus traversal before the event
         * reaches the focused node. All other events are passed to the focused
         * node's `on_key` handler.
         */
        void handleKeyEvent(const KeyEvent& event);

        // ------------------------------------------------------------------
        // Tab traversal
        // ------------------------------------------------------------------

        /** @brief Moves focus to the next registered node (wraps around). */
        void moveFocusForward();

        /** @brief Moves focus to the previous registered node (wraps around). */
        void moveFocusBackward();

        // ------------------------------------------------------------------
        // Global accessor
        // ------------------------------------------------------------------

        static void setActiveManager(FocusManager* manager) noexcept;
        static FocusManager* activeManager() noexcept;

    private:
        FocusNode*              current_focus_ = nullptr;
        std::vector<FocusNode*> focus_order_;

        static FocusManager* s_active_manager_;
    };

} // namespace systems::leal::campello_widgets
