#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
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
     * Supports IME (Input Method Editor) composition for entering complex
     * characters like accented letters (é, ñ) and CJK text.
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

        // Clears focused() if it still points to this instance -- a
        // TextField can be destroyed (e.g. its owning dialog closes)
        // without ever losing focus first, which would otherwise leave
        // s_focused_ dangling.
        ~TextEditingController();

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
        // IME Composition (Input Method Editor support)
        // ------------------------------------------------------------------

        /**
         * @brief Returns true if there is an active IME composition in progress.
         * 
         * When composing, the composing range indicates the text currently being
         * composed. This text is typically displayed with an underline and can be
         * modified by the IME until the user confirms the composition.
         */
        bool isComposing() const noexcept { return composing_start_ >= 0; }

        /** @brief Start of the composing range, or -1 if not composing. */
        int composingStart() const noexcept { return composing_start_; }

        /** @brief End of the composing range, or -1 if not composing. */
        int composingEnd() const noexcept { return composing_end_; }

        /**
         * @brief Returns the current composing text, or empty string if not composing.
         */
        std::string composingText() const;

        /**
         * @brief Begins a new IME composition at the current cursor position.
         * 
         * This is called by the platform when the user starts typing a composed
         * character (e.g., pressing a dead key like ´ for accents).
         */
        void beginComposing();

        /**
         * @brief Updates the composing text.
         * 
         * Replaces the current composing range with the new text and adjusts
         * the selection to be at the end of the composing text.
         * 
         * @param text The new composing text
         */
        void updateComposingText(std::string_view text);

        /**
         * @brief Sets the composing range and selection within it.
         * 
         * @param start Start of composing range (byte offset)
         * @param end End of composing range (byte offset)
         * @param selection_offset Selection offset from composing start
         */
        void setComposingRange(int start, int end, int selection_offset);

        /**
         * @brief Commits the current composing text.
         * 
         * The composing text becomes regular text and the composing state ends.
         * This is called when the user confirms the composition (e.g., pressing
         * Space or Enter).
         */
        void commitComposing();

        /**
         * @brief Cancels the current composition.
         * 
         * Removes the composing text and restores the selection to where
         * composition began.
         */
        void cancelComposing();

        // ------------------------------------------------------------------
        // Undo / redo
        // ------------------------------------------------------------------

        /**
         * @brief True if there is a prior edit state to restore.
         */
        bool canUndo() const noexcept { return !undo_stack_.empty(); }

        /**
         * @brief True if undo() has been called and there's a later state
         *        to restore.
         */
        bool canRedo() const noexcept { return !redo_stack_.empty(); }

        /**
         * @brief Restores the text/selection to before the most recent edit
         *        (or burst of edits — see insertText()'s coalescing note).
         *        No-op if canUndo() is false.
         */
        void undo();

        /** @brief Re-applies the most recently undone edit. No-op if canRedo() is false. */
        void redo();

        /**
         * @brief The TextEditingController currently attached to a focused
         *        TextField, if any.
         *
         * App-level Edit > Undo/Redo commands typically arrive as OS-level
         * menu key equivalents (Cmd+Z), which are consumed by the menu
         * system before they ever reach a focused widget's own key
         * handler. Checking this first lets such a command route to an
         * in-progress text edit instead of unconditionally hitting
         * document-level undo. Kept in sync by TextField's FocusNode
         * focus-change callback — not meant to be set from application code.
         */
        static TextEditingController* focused() noexcept { return s_focused_; }

        /**
         * @brief True if the currently-focused TextField has obscure_text
         *        set (a password-style field), false if nothing is focused.
         *
         * Whether a field is obscured is a widget-level property (the same
         * TextEditingController could in principle back different fields
         * over its lifetime), so it's tracked alongside the focused
         * pointer rather than on the controller itself. App-level
         * Edit > Cut/Copy should check this before touching the
         * clipboard — same reasoning a native password field never lets
         * its real contents get copied.
         */
        static bool focusedIsObscured() noexcept { return s_focused_obscured_; }

        /** @brief Sets/clears the focused() controller. Called by TextField's focus handling. */
        static void setFocused(TextEditingController* controller, bool obscured = false) noexcept
        {
            s_focused_          = controller;
            s_focused_obscured_ = controller ? obscured : false;
        }

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

        /**
         * @brief Pushes the current (pre-edit) text/selection onto the undo
         *        stack and clears the redo stack, ready for the caller to
         *        apply its mutation.
         *
         * Edits arriving within kUndoCoalesceWindowMs of the previous call
         * are merged into the same undo step instead of getting their own
         * — otherwise every single keystroke would be its own undo entry,
         * making Cmd+Z nearly useless while typing (mirrors Flutter's
         * EditableText undo grouping).
         */
        void recordUndoSnapshot();

        std::string text_;
        int selection_start_ = 0;
        int selection_end_   = 0;

        // IME composition state: -1 means not composing
        int composing_start_ = -1;
        int composing_end_   = -1;

        struct EditSnapshot
        {
            std::string text;
            int selection_start;
            int selection_end;
        };
        static constexpr uint64_t kUndoCoalesceWindowMs = 500;
        static constexpr size_t   kMaxUndoHistory        = 100;
        std::vector<EditSnapshot> undo_stack_;
        std::vector<EditSnapshot> redo_stack_;
        uint64_t last_snapshot_time_ms_ = 0;

        static TextEditingController* s_focused_;
        static bool s_focused_obscured_;

        uint64_t next_listener_id_ = 1;
        std::vector<std::pair<uint64_t, std::function<void()>>> listeners_;
        mutable std::mutex listeners_mutex_;
    };

} // namespace systems::leal::campello_widgets
