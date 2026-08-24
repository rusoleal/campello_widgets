#pragma once

#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief System clipboard access (text only).
     *
     * Implemented per platform in src/<platform>/clipboard.*. Currently a
     * real implementation exists only for macOS (NSPasteboard); other
     * platforms build against a stub that no-ops setText() and returns an
     * empty string from getText(), so the library still links everywhere,
     * but clipboard integration only actually works on macOS today.
     */
    class Clipboard
    {
    public:
        /** @brief Replaces the system clipboard's text contents. */
        static void setText(const std::string& text);

        /** @brief Returns the system clipboard's text contents, or empty string if unavailable/non-text. */
        static std::string getText();
    };

} // namespace systems::leal::campello_widgets
