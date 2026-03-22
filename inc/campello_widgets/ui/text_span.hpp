#pragma once

#include <string>
#include <campello_widgets/ui/text_style.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A run of text with a uniform style.
     *
     * TextSpan is the basic unit passed to RawText and to PaintContext draw
     * operations. Multi-styled text (inline spans) will be represented as
     * a tree of TextSpan objects in a later phase.
     */
    struct TextSpan
    {
        std::string text;
        TextStyle   style;

        bool operator==(const TextSpan&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
