#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/key_event.hpp>

#include <functional>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    class TextEditingController;

    /**
     * @brief RenderBox that draws a single-line text input and handles input.
     *
     * RenderTextField registers with PointerDispatcher for tap-to-place-cursor
     * and with the tick mechanism for cursor blinking. Keyboard events are
     * forwarded from the owning TextFieldState via handleKeyEvent().
     *
     * The cursor blink period is 530 ms on / 530 ms off. The cursor is always
     * shown (and the blink timer restarted) immediately after any text change
     * or focus gain.
     */
    class RenderTextField : public RenderBox
    {
    public:
        std::shared_ptr<TextEditingController> controller;

        TextStyle   style;
        Color       cursor_color          = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color       selection_color       = Color::fromRGBA(0.098f, 0.463f, 0.824f, 0.4f);
        Color       fill_color            = Color::white();
        Color       border_color          = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.38f);
        Color       focused_border_color  = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color       placeholder_color     = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.38f);
        std::string placeholder;

        float border_radius = 4.0f;
        float border_width  = 1.0f;
        float padding_h     = 12.0f; ///< Horizontal inner padding
        float padding_v     = 8.0f;  ///< Vertical inner padding
        float min_height    = 36.0f;

        bool focused      = false; ///< Synced from TextFieldState each rebuild
        bool obscure_text = false; ///< Draw bullets instead of characters

        /** Called after each text change (controller is already updated). */
        std::function<void(const std::string&)> on_changed;

        /** Called when Enter is pressed. */
        std::function<void(const std::string&)> on_submitted;

        /** Called on pointer-down tap so the owning State can request focus. */
        std::function<void()> on_tap;

        RenderTextField();
        ~RenderTextField() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        /**
         * @brief Forward a keyboard event from the FocusNode to this render object.
         * @return true if the event was consumed.
         */
        bool handleKeyEvent(const KeyEvent& event);

        /** @brief Reset the blink timer and show the cursor immediately. */
        void resetCursorBlink();

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        /** @brief Pixel width of text[0..byte_end). */
        float measurePrefix(int byte_end) const;

        /** @brief Returns the character byte-index closest to local x. */
        int hitTestText(float local_x) const;

        /** @brief Returns the display string (may replace chars with bullets). */
        std::string displayText() const;

        Offset   global_offset_;    ///< Latched in performPaint for pointer coords
        uint64_t last_blink_ms_  = 0;
        bool     cursor_visible_ = true;
        bool     pressed_        = false; ///< True while pointer is down
    };

} // namespace systems::leal::campello_widgets
