#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

namespace systems::leal::campello_widgets
{

    class TextEditingController;

    /**
     * @brief Manages the connection between platform IME and the active text field.
     *
     * TextInputManager provides a bridge between platform-specific input methods
     * (like NSTextInputClient on macOS or IMM on Windows) and the widget layer's
     * TextEditingController.
     *
     * When a TextField gains focus, it registers itself as the input target.
     * When it loses focus, it unregisters.
     *
     * Platform adapters query this manager to find the active controller and
     * send IME composition events to it.
     */
    class TextInputManager
    {
    public:
        /**
         * @brief Information about the current text input target.
         */
        struct InputTarget
        {
            TextEditingController* controller = nullptr;
            
            // Optional: callback to get the character rect for IME candidate window positioning
            // Parameters: byte offset into text, returns: rect in screen coordinates {x, y, w, h}
            std::function<std::array<float, 4>(int)> get_character_rect;

            // Optional: callback to map a screen point to a character index
            // Parameters: point in screen coordinates (x, y), returns: byte offset into text
            std::function<int(float x, float y)> get_position_for_point;
        };

        /** @brief Sets the global active TextInputManager instance. */
        static void setActiveManager(TextInputManager* manager) noexcept;

        /** @brief Returns the global active TextInputManager instance. */
        static TextInputManager* activeManager() noexcept;

        TextInputManager() = default;
        ~TextInputManager() = default;

        // Non-copyable, non-movable
        TextInputManager(const TextInputManager&) = delete;
        TextInputManager& operator=(const TextInputManager&) = delete;

        /**
         * @brief Registers a text field as the active input target.
         * 
         * Called by TextField when it gains focus.
         * 
         * @param target Information about the input target
         */
        void registerInputTarget(const InputTarget& target);

        /**
         * @brief Unregisters the current input target.
         * 
         * Called by TextField when it loses focus.
         */
        void unregisterInputTarget();

        /**
         * @brief Returns true if there is an active input target.
         */
        bool hasInputTarget() const noexcept { return current_target_.controller != nullptr; }

        /**
         * @brief Returns the current input target, or nullptr if none.
         */
        const InputTarget* currentTarget() const noexcept 
        { 
            return hasInputTarget() ? &current_target_ : nullptr; 
        }

        /**
         * @brief Convenience method to get the active controller.
         * @return The active controller, or nullptr if none.
         */
        TextEditingController* activeController() const noexcept
        {
            return current_target_.controller;
        }

        /**
         * @brief Begins IME composition on the active target.
         * @return true if composition was started, false if no active target.
         */
        bool beginComposing();

        /**
         * @brief Updates the composing text on the active target.
         * @param text The new composing text
         * @return true if the text was updated, false if no active target.
         */
        bool updateComposingText(std::string_view text);

        /**
         * @brief Commits the current composition.
         * @return true if composition was committed, false if no active target.
         */
        bool commitComposing();

        /**
         * @brief Cancels the current composition.
         * @return true if composition was cancelled, false if no active target.
         */
        bool cancelComposing();

        /**
         * @brief Inserts text directly (bypassing composition).
         * @param text The text to insert
         * @return true if text was inserted, false if no active target.
         */
        bool insertText(std::string_view text);

        /**
         * @brief Returns true if the active target is currently composing.
         */
        bool isComposing() const;

        /**
         * @brief Registers a callback fired whenever an input target is registered
         *        or unregistered.
         * 
         * This is used by platform adapters (e.g. iOS) to show or hide the
         * software keyboard when a text field gains or loses focus.
         */
        void setOnInputTargetChanged(std::function<void(bool has_target)> callback);

        /**
         * @brief Gets the character rect for positioning the IME candidate window.
         * @param byte_offset The byte offset in the text
         * @return A rect {x, y, width, height} in window coordinates, or all zeros if no target.
         */
        std::array<float, 4> getCharacterRect(int byte_offset) const;

        /**
         * @brief Maps a point in window coordinates to a character index.
         * @param x X coordinate in window coordinates
         * @param y Y coordinate in window coordinates
         * @return Byte offset into the text, or 0 if no active target.
         */
        int getPositionForPoint(float x, float y) const;

    private:
        InputTarget current_target_;
        std::function<void(bool)> on_target_changed_;
        static TextInputManager* s_active_manager_;
    };

} // namespace systems::leal::campello_widgets
