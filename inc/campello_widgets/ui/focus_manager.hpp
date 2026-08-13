#pragma once

#include <atomic>
#include <functional>
#include <mutex>
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
         * The global key handler (see setGlobalKeyHandler()), if any, is
         * checked first and may consume the event outright. Otherwise,
         * Tab / Shift+Tab are intercepted for focus traversal before the
         * event reaches the focused node. All other events are passed to
         * the focused node's `on_key` handler.
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

        // ------------------------------------------------------------------
        // App-level shortcuts
        // ------------------------------------------------------------------

        /**
         * @brief Registers a handler checked before any focus-based
         * routing, for shortcuts that must fire regardless of what
         * currently has keyboard focus (e.g. an app-wide Cmd/Ctrl+<key>
         * dev toggle that shouldn't stop working just because a text
         * field elsewhere is focused).
         *
         * Every platform adapter's key event ultimately reaches
         * handleKeyEvent() (macOS, Windows, Linux/X11, Linux/Wayland,
         * Android all call it directly), so this one hook applies
         * uniformly across platforms with no per-platform wiring needed.
         *
         * The handler returns true to consume the event (skips Tab
         * traversal and focus routing for that event) or false to let it
         * fall through to normal handling.
         */
        static void setGlobalKeyHandler(std::function<bool(const KeyEvent&)> handler);

        /**
         * @brief Returns the currently installed global key handler, or an
         * empty std::function if none is set.
         *
         * Lets a second caller compose with whatever handler is already
         * installed — save this before calling setGlobalKeyHandler(),
         * chain to it from the new handler, and restore it on teardown —
         * instead of silently clobbering it. See PlatformMenuBarView for
         * an example: it needs the single global-handler slot for menu
         * accelerators without breaking an app's own pre-existing handler
         * (e.g. dev-only shortcuts registered before the menu bar mounts).
         */
        static std::function<bool(const KeyEvent&)> globalKeyHandler();

    private:
        FocusNode*              current_focus_ = nullptr;
        std::vector<FocusNode*> focus_order_;

        static std::atomic<FocusManager*> s_active_manager_;

        static std::mutex                            s_global_handler_mutex_;
        static std::function<bool(const KeyEvent&)>  s_global_key_handler_;
    };

} // namespace systems::leal::campello_widgets
