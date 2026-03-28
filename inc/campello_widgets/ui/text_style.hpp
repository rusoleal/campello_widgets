#pragma once

#include <string>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Horizontal alignment for text.
     */
    enum class TextAlign
    {
        left,    /**< Align text to the left edge. */
        right,   /**< Align text to the right edge. */
        center,  /**< Center text horizontally. */
        start,   /**< Align to start (left for LTR, right for RTL). */
        end,     /**< Align to end (right for LTR, left for RTL). */
        justify, /**< Stretch spaces to fill width (not yet implemented). */
    };

    /**
     * @brief Font weight for text.
     */
    enum class FontWeight
    {
        normal = 400,
        bold   = 700,
    };

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

        /** @brief Font weight. */
        FontWeight font_weight = FontWeight::normal;

        /** @brief Whether text is italic. */
        bool italic = false;

        // Phase 5 stub — weight, italic, letter-spacing will be added
        // when font atlas rendering is implemented.

        bool operator==(const TextStyle&) const noexcept = default;

        // Builder-style methods for fluent API
        TextStyle& withColor(Color c) { color = c; return *this; }
        TextStyle& withFontSize(float size) { font_size = size; return *this; }
        TextStyle& withFontFamily(std::string family) { font_family = std::move(family); return *this; }
        TextStyle& bold() { font_weight = FontWeight::bold; return *this; }
        TextStyle& withItalic(bool i = true) { italic = i; return *this; }

        /**
         * @brief Merges another style into this one, keeping non-default values.
         *
         * The child's non-default values override parent values.
         */
        TextStyle merge(const TextStyle& child) const
        {
            TextStyle result = *this;
            if (child.color != Color::black()) result.color = child.color;
            if (child.font_size != 14.0f) result.font_size = child.font_size;
            if (!child.font_family.empty()) result.font_family = child.font_family;
            if (child.font_weight != FontWeight::normal) result.font_weight = child.font_weight;
            if (child.italic) result.italic = child.italic;
            return result;
        }
    };

} // namespace systems::leal::campello_widgets
