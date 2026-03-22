#pragma once

#include <string>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Visual styling properties for a run of text.
     */
    struct TextStyle
    {
        /** @brief Text fill color. */
        Color color = Color::black();

        /** @brief Font size in logical pixels. */
        float font_size = 14.0f;

        /**
         * @brief Font family name (empty = platform default).
         *
         * Font loading and fallback resolution will be implemented in
         * the text rendering phase.
         */
        std::string font_family;

        // Phase 5 stub — weight, italic, letter-spacing will be added
        // when font atlas rendering is implemented.

        bool operator==(const TextStyle&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
