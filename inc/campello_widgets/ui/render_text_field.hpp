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
     * @brief RenderBox that draws a text input (single or multi-line) and handles input.
     *
     * RenderTextField registers with PointerDispatcher for tap-to-place-cursor
     * and with the tick mechanism for cursor blinking. Keyboard events are
     * forwarded from the owning TextFieldState via handleKeyEvent().
     *
     * The cursor blink period is 530 ms on / 530 ms off. The cursor is always
     * shown (and the blink timer restarted) immediately after any text change
     * or focus gain.
     *
     * ## Multi-line support
     * When max_lines != 1, the text field supports multi-line editing:
     * - Text wraps to multiple lines
     * - Vertical scrolling when content exceeds viewport
     * - Cursor navigation across lines (up/down arrows)
     * - Newlines inserted via Enter key
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

        // Multi-line configuration
        int  max_lines = 1;  ///< 1 = single-line, >1 = multi-line with limit, 0 = unlimited
        int  min_lines = 1;  ///< Minimum number of lines to show (multi-line only)
        bool expands   = false; ///< Whether to expand to fill parent (multi-line only)

        /** Called after each text change (controller is already updated). */
        std::function<void(const std::string&)> on_changed;

        /** Called when Enter is pressed (single-line) or Ctrl+Enter (multi-line). */
        std::function<void(const std::string&)> on_submitted;

        /** Called on pointer-down tap so the owning State can request focus. */
        std::function<void()> on_tap;

        RenderTextField();
        ~RenderTextField() override;

        void attach() override;
        void detach() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        /**
         * @brief Forward a keyboard event from the FocusNode to this render object.
         * @return true if the event was consumed.
         */
        bool handleKeyEvent(const KeyEvent& event);

        /** @brief Reset the blink timer and show the cursor immediately. */
        void resetCursorBlink();

        /** @brief Returns true if this is a multi-line text field. */
        bool isMultiline() const { return max_lines != 1; }

        /**
         * @brief Returns the bounding rect of a character range in local coordinates.
         *
         * The rect is relative to the render object's origin and includes padding.
         * Used by the platform IME to position candidate windows.
         */
        Rect getRectForCharacterRange(int start, int end) const;

        /**
         * @brief Returns the character index at a local point.
         *
         * Coordinates are relative to the render object's origin (including padding).
         * Used by the platform IME for hit-testing and cursor placement.
         */
        int getPositionForPoint(float local_x, float local_y) const;

        /** @brief The global offset latched during the last paint. */
        Offset globalOffset() const noexcept { return global_offset_; }

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        // Text measurement helpers
        float measurePrefix(int byte_end) const;
        float measureText(const std::string& text) const;
        float textHeight() const;
        float lineHeight() const;

        // Layout helpers for multi-line
        void layoutMultiline();
        int  getLineCount() const;
        int  getLineForPosition(int byte_pos) const;
        int  getPositionForLine(int line) const;
        float getLineWidth(int line) const;

        // Hit testing
        int hitTestText(float local_x, float local_y) const;
        int hitTestSingleLine(float local_x) const;

        // Drawing helpers
        std::string displayText() const;
        void drawTextLine(PaintContext& ctx, const std::string& line, const Offset& offset, float y);
        void drawCursor(PaintContext& ctx, float x, float y);
        void drawSelection(PaintContext& ctx, int sel_start, int sel_end, float y, float line_h);

        Offset   global_offset_;    ///< Latched in performPaint for pointer coords
        uint64_t last_blink_ms_  = 0;
        bool     cursor_visible_ = true;
        bool     pressed_        = false; ///< True while pointer is down
        int32_t  pointer_id_     = 0;      ///< Pointer ID that pressed this field

        // Multi-line state
        mutable std::vector<std::string> lines_; ///< Cached wrapped lines
        float scroll_offset_y_ = 0.0f;           ///< Vertical scroll offset

        void applyScrollDelta(float delta);
    };

} // namespace systems::leal::campello_widgets
