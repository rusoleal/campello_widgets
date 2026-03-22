#pragma once

#include <cstdint>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Platform-agnostic logical key identifiers.
     *
     * Values are stable across platforms. The platform adapter maps native
     * keycodes to these before sending a KeyEvent.
     */
    enum class KeyCode : uint32_t
    {
        unknown = 0,

        // Letters
        a, b, c, d, e, f, g, h, i, j, k, l, m,
        n, o, p, q, r, s, t, u, v, w, x, y, z,

        // Digits (top row)
        digit_0, digit_1, digit_2, digit_3, digit_4,
        digit_5, digit_6, digit_7, digit_8, digit_9,

        // Whitespace / editing
        space,
        enter,
        tab,
        backspace,
        escape,
        delete_forward,

        // Navigation
        left, right, up, down,
        home, end,
        page_up, page_down,

        // Function keys
        f1, f2, f3, f4, f5, f6,
        f7, f8, f9, f10, f11, f12,

        // Modifier keys (as primary key events, e.g. pressing Shift alone)
        left_shift,  right_shift,
        left_ctrl,   right_ctrl,
        left_alt,    right_alt,
        left_meta,   right_meta, ///< Cmd on macOS, Win on Windows
        caps_lock,
    };

    /**
     * @brief Bitmask of active modifier keys.
     *
     * Combine with bitwise OR: `KeyModifiers::shift | KeyModifiers::ctrl`.
     */
    struct KeyModifiers
    {
        static constexpr uint32_t none  = 0;
        static constexpr uint32_t shift = 1u << 0;
        static constexpr uint32_t ctrl  = 1u << 1;
        static constexpr uint32_t alt   = 1u << 2; ///< Option on macOS
        static constexpr uint32_t meta  = 1u << 3; ///< Cmd on macOS, Win on Windows
    };

    /** @brief Whether the key was pressed, released, or is auto-repeating. */
    enum class KeyEventKind : uint8_t
    {
        down,
        up,
        repeat, ///< Key held down; platform is auto-repeating keyDown
    };

    /**
     * @brief A single platform-agnostic keyboard event.
     *
     * Produced by platform adapters and routed through FocusManager to the
     * currently focused widget.
     */
    struct KeyEvent
    {
        KeyEventKind kind      = KeyEventKind::down;
        KeyCode      key_code  = KeyCode::unknown;
        uint32_t     modifiers = KeyModifiers::none; ///< Active modifier bitmask
        uint32_t     character = 0; ///< Unicode code point; 0 for non-printable keys
    };

} // namespace systems::leal::campello_widgets
