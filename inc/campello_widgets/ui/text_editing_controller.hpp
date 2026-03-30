#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Controls the text and selection state of a TextField.
     *
     * TextEditingController is created by the user and passed to a TextField.
     * The TextField syncs its display to the controller's state and calls
     * the controller's mutating methods in response to user input.
     *
     * Typical use:
     * @code
     * auto ctrl = std::make_shared<TextEditingController>();
     * ctrl->addListener([&] { setState([]{}); });
     * // ... pass ctrl to TextField ...
     * std::string text = ctrl->text();
     * @endcode
     */
    class TextEditingController
    {
    public:
        TextEditingController() = default;
        explicit TextEditingController(std::string initial_text);

        // Non-copyable — owns listener state.
        TextEditingController(const TextEditingController&)            = delete;
        TextEditingController& operator=(const TextEditingController&) = delete;

        // ------------------------------------------------------------------
        // Text
        // ------------------------------------------------------------------

        const std::string& text() const noexcept { return text_; }

        /** @brief Replaces all text and moves cursor to end. */
        void setText(std::string text);

        /** @brief Clears all text and resets selection. */
        void clear();

        // ------------------------------------------------------------------
        // Selection (byte offsets into text())
        // ------------------------------------------------------------------

        /** @brief Start of selection, or cursor position if collapsed. */
        int selectionStart() const noexcept { return selection_start_; }

        /** @brief End of selection (== selectionStart when cursor is collapsed). */
        int selectionEnd() const noexcept { return selection_end_; }

        bool hasSelection() const noexcept { return selection_start_ != selection_end_; }

        /** @brief Sets selection range. Pass equal values for a collapsed cursor. */
        void setSelection(int start, int end);

        /** @brief Selects all text. */
        void selectAll();

        /** @brief Returns the currently selected text, or empty string. */
        std::string selectedText() const;

        // ------------------------------------------------------------------
        // Editing helpers (called by RenderTextField)
        // ------------------------------------------------------------------

        /** @brief Inserts text at cursor / replaces current selection. */
        void insertText(std::string_view to_insert);

        /** @brief Deletes the character before the cursor (or clears selection). */
        void deleteBackward();

        /** @brief Deletes the character after the cursor (or clears selection). */
        void deleteForward();

        // ------------------------------------------------------------------
        // Listener API
        // ------------------------------------------------------------------

        /**
         * @brief Registers a callback fired whenever text or selection changes.
         * @return A listener ID for use with removeListener().
         */
        uint64_t addListener(std::function<void()> fn);

        /** @brief Removes the listener with the given ID. */
        void removeListener(uint64_t id);

    private:
        void notifyListeners();

        std::string text_;
        int selection_start_ = 0;
        int selection_end_   = 0;

        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;
    };

} // namespace systems::leal::campello_widgets
