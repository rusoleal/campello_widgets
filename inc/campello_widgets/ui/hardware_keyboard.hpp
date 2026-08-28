#pragma once

#include <atomic>
#include <cstdint>
#include <campello_widgets/ui/key_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Live "what modifier keys are currently held" state, queryable
     * from anywhere -- mirrors Flutter's `HardwareKeyboard.instance`.
     *
     * `PointerEvent` deliberately has no modifier field (matching Flutter's
     * own `PointerEvent`, which doesn't either) -- Cmd+Click and similar
     * modifier+pointer combinations are detected by checking
     * `HardwareKeyboard::current()` from inside a pointer/gesture handler,
     * the same pattern Flutter apps use (`HardwareKeyboard.instance.isMetaPressed`
     * inside `onTapDown`, etc.).
     *
     * State is kept current by `FocusManager::handleKeyEvent()` (every
     * `KeyEvent` updates it) and, on macOS, by a `flagsChanged:` handler for
     * bare modifier presses (Cmd alone, no other key) that never produce a
     * `keyDown:`/`keyUp:` at all -- see `updateModifiers()`'s doc comment.
     * Application code should only ever read this, never call
     * `updateModifiers()` directly.
     *
     * Tracks only the coarse `KeyModifiers` bitmask (shift/ctrl/alt/meta),
     * not per-key (left/right) state: `KeyCode`'s `left_shift`/`right_shift`/
     * etc. values exist but are unreliably populated across platforms for
     * modifier keys specifically (e.g. macOS never emits a discrete
     * modifier-only `KeyCode`, and Windows never maps Cmd/Win to anything
     * but `KeyCode::unknown`) -- this scope matches what Flutter's own
     * `isShiftPressed`/`isControlPressed`/`isAltPressed`/`isMetaPressed`
     * convenience getters expose.
     */
    class HardwareKeyboard
    {
    public:
        /** @brief The single process-wide instance. */
        static HardwareKeyboard& current() noexcept;

        /**
         * @brief Replaces the tracked modifier state with `modifiers`.
         *
         * Called by `FocusManager::handleKeyEvent()` for every `KeyEvent`
         * (covers the common case: any modifier change that coincides with
         * another key), and directly by platform adapters for modifier
         * transitions that never produce a `KeyEvent` at all -- e.g. macOS's
         * `flagsChanged:` for a bare Cmd/Shift/Ctrl/Option press or release
         * with no other key involved. Not intended to be called by
         * application code.
         */
        void updateModifiers(uint32_t modifiers) noexcept;

        /** @brief The current modifier bitmask -- see `KeyModifiers`. */
        uint32_t modifiers() const noexcept;

        bool isShiftPressed()   const noexcept;
        bool isControlPressed() const noexcept;
        bool isAltPressed()     const noexcept; ///< Option on macOS.
        bool isMetaPressed()    const noexcept; ///< Cmd on macOS, Win key on Windows.

    private:
        HardwareKeyboard() = default;

        std::atomic<uint32_t> modifiers_{KeyModifiers::none};
    };

} // namespace systems::leal::campello_widgets
