#pragma once

#include <cstddef>
#include <functional>
#include <campello_widgets/ui/key_event.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A key + modifier-bitmask combination, for matching against a
     * `KeyEvent` -- e.g. `KeyCombo{KeyCode::s, KeyModifiers::ctrl}` for Ctrl+S.
     */
    struct KeyCombo
    {
        KeyCode  key_code  = KeyCode::unknown;
        uint32_t modifiers = KeyModifiers::none;

        bool operator==(const KeyCombo&) const noexcept = default;
    };

    /** @brief Hasher for KeyCombo, e.g. for use as an unordered_map key. */
    struct KeyComboHash
    {
        size_t operator()(const KeyCombo& k) const noexcept
        {
            return std::hash<uint32_t>{}(static_cast<uint32_t>(k.key_code)) ^
                   (std::hash<uint32_t>{}(k.modifiers) << 1);
        }
    };

} // namespace systems::leal::campello_widgets
