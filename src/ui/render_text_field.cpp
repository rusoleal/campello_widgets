#include <campello_widgets/ui/render_text_field.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

namespace
{
    // Color for composing text underline (typically a darker blue/gray)
    constexpr uint32_t kComposingUnderlineColor = 0xFF2196F3;  // Material Blue
}

namespace systems::leal::campello_widgets
{

    static constexpr uint64_t kBlinkPeriodMs = 530;
    static constexpr float    kCursorWidth   = 2.0f;

    // -------------------------------------------------------------------------

    RenderTextField::RenderTextField() = default;

    RenderTextField::~RenderTextField()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    void RenderTextField::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this,     [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now_ms)        { onTick(now_ms);   });
        }
    }

    void RenderTextField::detach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    void RenderTextField::performLayout()
    {
        if (isMultiline())
        {
            layoutMultiline();
        }
        else
        {
            // Single-line layout (original behavior)
            float line_height = this->lineHeight();
            float natural_h   = std::max(min_height, line_height + padding_v * 2.0f);

            float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
            float h = std::isinf(constraints_.max_height) ? natural_h
                                                           : std::min(natural_h, constraints_.max_height);
            size_ = constraints_.constrain({w, h});
        }
    }

    void RenderTextField::layoutMultiline()
    {
        // Clear line cache before layout to ensure fresh wrapping calculation
        lines_.clear();

        float line_h = lineHeight();
        float iw = constraints_.max_width - padding_h * 2.0f;
        if (iw < 0) iw = 0;

        // Split text into lines
        getLineCount(); // This populates lines_
        int num_lines = static_cast<int>(lines_.size());
        if (num_lines == 0) num_lines = 1;

        // Calculate height based on lines
        int visible_lines = num_lines;
        if (max_lines > 0)
        {
            visible_lines = std::min(num_lines, max_lines);
        }
        visible_lines = std::max(visible_lines, min_lines);

        float content_h = visible_lines * line_h;
        float natural_h = std::max(min_height, content_h + padding_v * 2.0f);

        // Handle expands
        float h;
        if (expands && !std::isinf(constraints_.max_height))
        {
            h = constraints_.max_height;
        }
        else
        {
            h = std::min(natural_h, std::isinf(constraints_.max_height) ? natural_h : constraints_.max_height);
        }

        float w = std::isinf(constraints_.max_width) ? 0.0f : constraints_.max_width;
        size_ = constraints_.constrain({w, h});
    }

    // -------------------------------------------------------------------------
    // Paint
    // -------------------------------------------------------------------------

    void RenderTextField::performPaint(PaintContext& ctx, const Offset& offset)
    {
        global_offset_ = offset;
        Canvas& canvas = ctx.canvas();

        const float w = size_.width;
        const float h = size_.height;

        // --- Background ---
        RRect bg{ Rect::fromLTWH(offset.x, offset.y, w, h), border_radius };
        canvas.drawRRect(bg, Paint::filled(fill_color));

        // --- Border ---
        Color bc = focused ? focused_border_color : border_color;
        canvas.drawRRect(bg, Paint::stroked(bc, border_width));

        // --- Clip to inner text area ---
        const float ix = offset.x + padding_h;
        const float iy = offset.y + padding_v;
        const float iw = w - padding_h * 2.0f;
        const float ih = h - padding_v * 2.0f;
        canvas.save();
        canvas.clipRect(Rect::fromLTWH(ix, iy, iw, ih));

        const std::string disp = displayText();
        const bool empty = disp.empty();
        float line_h = lineHeight();

        // Scale font to physical pixels for rendering, matching what RenderParagraph does.
        // measureText/measurePrefix use the logical style for cursor/selection positioning,
        // so the drawn text and the cursor stay aligned regardless of DPR.
        const float dpr = activeDevicePixelRatio();
        TextStyle scaled_style = style;
        scaled_style.font_size *= dpr;

        if (isMultiline())
        {
            // Multi-line rendering
            if (empty && !placeholder.empty())
            {
                TextStyle ph_style = scaled_style;
                ph_style.color = placeholder_color;
                canvas.drawText(TextSpan{placeholder, ph_style}, {ix, iy});
            }
            else if (!empty)
            {
                // Get wrapped lines
                getLineCount();

                // Apply scroll offset
                float ty = iy - scroll_offset_y_;

                // Draw each line
                int start_line = static_cast<int>(scroll_offset_y_ / line_h);
                int end_line = std::min(static_cast<int>(lines_.size()),
                                       start_line + static_cast<int>(ih / line_h) + 2);

                int char_pos = 0;
                for (int i = 0; i < start_line && i < static_cast<int>(lines_.size()); ++i)
                {
                    char_pos += static_cast<int>(lines_[i].size()) + 1; // +1 for newline
                }

                for (int line_idx = start_line; line_idx < end_line && line_idx < static_cast<int>(lines_.size()); ++line_idx)
                {
                    const std::string& line = lines_[line_idx];
                    float line_y = ty + line_idx * line_h;

                    // Skip if outside visible area
                    if (line_y + line_h < iy || line_y > iy + ih)
                    {
                        char_pos += static_cast<int>(line.size()) + 1;
                        continue;
                    }

                    int line_start_pos = char_pos;
                    int line_end_pos = char_pos + static_cast<int>(line.size());

                    // Selection highlight for this line
                    if (controller && controller->hasSelection())
                    {
                        int sel_start = std::min(controller->selectionStart(), controller->selectionEnd());
                        int sel_end = std::max(controller->selectionStart(), controller->selectionEnd());

                        if (sel_end > line_start_pos && sel_start < line_end_pos)
                        {
                            int sel_start_in_line = std::max(sel_start, line_start_pos) - line_start_pos;
                            int sel_end_in_line = std::min(sel_end, line_end_pos) - line_start_pos;

                            float sel_x0 = ix + measureText(line.substr(0, sel_start_in_line));
                            float sel_x1 = ix + measureText(line.substr(0, sel_end_in_line));

                            canvas.drawRect(
                                Rect::fromLTWH(sel_x0, line_y, sel_x1 - sel_x0, line_h),
                                Paint::filled(selection_color));
                        }
                    }

                    // Draw line text
                    if (!line.empty())
                    {
                        canvas.drawText(TextSpan{line, scaled_style}, {ix, line_y});
                    }

                    // Draw composing underline for text on this line
                    if (controller && controller->isComposing())
                    {
                        int comp_start = controller->composingStart();
                        int comp_end = controller->composingEnd();
                        
                        // Check if composing range intersects with this line
                        if (comp_start < line_end_pos && comp_end > line_start_pos)
                        {
                            int comp_start_in_line = std::max(comp_start, line_start_pos) - line_start_pos;
                            int comp_end_in_line = std::min(comp_end, line_end_pos) - line_start_pos;
                            
                            float comp_x0 = ix + measureText(line.substr(0, comp_start_in_line));
                            float comp_x1 = ix + measureText(line.substr(0, comp_end_in_line));
                            float underline_y = line_y + line_h - 3.0f;
                            
                            Paint underline_paint = Paint::stroked(
                                Color::fromARGB(kComposingUnderlineColor),
                                1.5f
                            );
                            canvas.drawLine(
                                Offset{comp_x0, underline_y},
                                Offset{comp_x1, underline_y},
                                underline_paint
                            );
                        }
                    }

                    // Draw cursor if on this line
                    if (focused && cursor_visible_ && controller)
                    {
                        int cursor_pos = controller->selectionEnd();
                        if (cursor_pos >= line_start_pos && cursor_pos <= line_end_pos)
                        {
                            int cursor_in_line = cursor_pos - line_start_pos;
                            float cx = ix + measureText(line.substr(0, cursor_in_line));
                            canvas.drawRect(
                                Rect::fromLTWH(cx, line_y, kCursorWidth, line_h),
                                Paint::filled(cursor_color));
                        }
                    }

                    char_pos = line_end_pos + 1; // +1 for newline
                }
            }
        }
        else
        {
            // Single-line rendering (original behavior)
            float ty = iy + (ih - line_h) * 0.5f;

            if (empty && !placeholder.empty())
            {
                TextStyle ph_style = scaled_style;
                ph_style.color = placeholder_color;
                canvas.drawText(TextSpan{placeholder, ph_style}, {ix, ty});
            }
            else if (!empty)
            {
                // Selection highlight
                if (controller && controller->hasSelection())
                {
                    int lo = std::min(controller->selectionStart(), controller->selectionEnd());
                    int hi = std::max(controller->selectionStart(), controller->selectionEnd());
                    float sel_x0 = ix + measurePrefix(lo);
                    float sel_x1 = ix + measurePrefix(hi);
                    canvas.drawRect(
                        Rect::fromLTWH(sel_x0, ty, sel_x1 - sel_x0, line_h),
                        Paint::filled(selection_color));
                }

                // Text
                canvas.drawText(TextSpan{disp, scaled_style}, {ix, ty});

                // Composing text underline (IME composition visual feedback)
                if (controller && controller->isComposing())
                {
                    int comp_start = controller->composingStart();
                    int comp_end = controller->composingEnd();
                    
                    float comp_x0 = ix + measurePrefix(comp_start);
                    float comp_x1 = ix + measurePrefix(comp_end);
                    float underline_y = ty + line_h - 3.0f;  // 3px from bottom
                    
                    // Draw underline for composing text
                    Paint underline_paint = Paint::stroked(
                        Color::fromARGB(kComposingUnderlineColor), 
                        1.5f  // underline thickness
                    );
                    canvas.drawLine(
                        Offset{comp_x0, underline_y},
                        Offset{comp_x1, underline_y},
                        underline_paint
                    );
                }

                // Cursor
                if (focused && cursor_visible_ && controller)
                {
                    int cursor_pos = controller->selectionEnd();
                    float cx = ix + measurePrefix(cursor_pos);
                    canvas.drawRect(
                        Rect::fromLTWH(cx, ty, kCursorWidth, line_h),
                        Paint::filled(cursor_color));
                }
            }
        }

        canvas.restore();
    }

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------

    bool RenderTextField::handleKeyEvent(const KeyEvent& event)
    {
        if (!controller) return false;
        if (event.kind == KeyEventKind::up) return false;

        const bool shift = (event.modifiers & KeyModifiers::shift) != 0;
        const bool ctrl  = (event.modifiers & KeyModifiers::ctrl)  != 0
                        || (event.modifiers & KeyModifiers::meta)  != 0;

        switch (event.key_code)
        {
        case KeyCode::backspace:
            controller->deleteBackward();
            if (on_changed) on_changed(controller->text());
            resetCursorBlink();
            return true;

        case KeyCode::delete_forward:
            controller->deleteForward();
            if (on_changed) on_changed(controller->text());
            resetCursorBlink();
            return true;

        case KeyCode::enter:
            if (isMultiline() && !ctrl)
            {
                // Multi-line: Insert newline
                controller->insertText("\n");
                if (on_changed) on_changed(controller->text());
                resetCursorBlink();
            }
            else if (on_submitted)
            {
                // Single-line, or multi-line with Ctrl+Enter: Submit
                on_submitted(controller->text());
            }
            return true;

        case KeyCode::left:
        {
            int pos = controller->selectionEnd();
            if (ctrl)
            {
                // Jump to start of previous word
                const auto& t = controller->text();
                while (pos > 0 && t[static_cast<size_t>(pos - 1)] == ' ') --pos;
                while (pos > 0 && t[static_cast<size_t>(pos - 1)] != ' ') --pos;
            }
            else
            {
                pos = std::max(0, pos - 1);
            }
            controller->setSelection(shift ? controller->selectionStart() : pos, pos);
            resetCursorBlink();
            return true;
        }

        case KeyCode::right:
        {
            int pos = controller->selectionEnd();
            int sz  = static_cast<int>(controller->text().size());
            if (ctrl)
            {
                const auto& t = controller->text();
                while (pos < sz && t[static_cast<size_t>(pos)] != ' ') ++pos;
                while (pos < sz && t[static_cast<size_t>(pos)] == ' ') ++pos;
            }
            else
            {
                pos = std::min(sz, pos + 1);
            }
            controller->setSelection(shift ? controller->selectionStart() : pos, pos);
            resetCursorBlink();
            return true;
        }

        case KeyCode::up:
            if (isMultiline())
            {
                // Move cursor up one line
                int pos = controller->selectionEnd();
                const auto& t = controller->text();

                // Find current line start
                int line_start = pos;
                while (line_start > 0 && t[static_cast<size_t>(line_start - 1)] != '\n') --line_start;

                // Find current column
                int col = pos - line_start;

                if (line_start > 0)
                {
                    // Find previous line start
                    int prev_line_start = line_start - 1;
                    while (prev_line_start > 0 && t[static_cast<size_t>(prev_line_start - 1)] != '\n') --prev_line_start;

                    // Find previous line end (which is current line_start - 1)
                    int prev_line_end = line_start - 1;

                    // Move to same column on previous line (or end of line if shorter)
                    int new_pos = std::min(prev_line_start + col, prev_line_end);
                    controller->setSelection(shift ? controller->selectionStart() : new_pos, new_pos);
                    resetCursorBlink();
                }
                return true;
            }
            break;

        case KeyCode::down:
            if (isMultiline())
            {
                // Move cursor down one line
                int pos = controller->selectionEnd();
                const auto& t = controller->text();
                int sz = static_cast<int>(t.size());

                // Find current line start
                int line_start = pos;
                while (line_start > 0 && t[static_cast<size_t>(line_start - 1)] != '\n') --line_start;

                // Find current column
                int col = pos - line_start;

                // Find end of current line
                int line_end = pos;
                while (line_end < sz && t[static_cast<size_t>(line_end)] != '\n') ++line_end;

                if (line_end < sz)
                {
                    // Move to next line
                    int next_line_start = line_end + 1;
                    int next_line_end = next_line_start;
                    while (next_line_end < sz && t[static_cast<size_t>(next_line_end)] != '\n') ++next_line_end;

                    // Move to same column on next line (or end of line if shorter)
                    int new_pos = std::min(next_line_start + col, next_line_end);
                    controller->setSelection(shift ? controller->selectionStart() : new_pos, new_pos);
                    resetCursorBlink();
                }
                return true;
            }
            break;

        case KeyCode::home:
            if (isMultiline())
            {
                // Move to start of current line
                int pos = controller->selectionEnd();
                const auto& t = controller->text();
                int line_start = pos;
                while (line_start > 0 && t[static_cast<size_t>(line_start - 1)] != '\n') --line_start;
                controller->setSelection(shift ? controller->selectionStart() : line_start, line_start);
            }
            else
            {
                controller->setSelection(shift ? controller->selectionStart() : 0, 0);
            }
            resetCursorBlink();
            return true;

        case KeyCode::end:
        {
            if (isMultiline())
            {
                // Move to end of current line
                int pos = controller->selectionEnd();
                const auto& t = controller->text();
                int sz = static_cast<int>(t.size());
                int line_end = pos;
                while (line_end < sz && t[static_cast<size_t>(line_end)] != '\n') ++line_end;
                controller->setSelection(shift ? controller->selectionStart() : line_end, line_end);
            }
            else
            {
                int end = static_cast<int>(controller->text().size());
                controller->setSelection(shift ? controller->selectionStart() : end, end);
            }
            resetCursorBlink();
            return true;
        }

        case KeyCode::a:
            if (ctrl)
            {
                controller->selectAll();
                resetCursorBlink();
                return true;
            }
            break;

        default:
            break;
        }

        // Printable character
        if (event.character != 0)
        {
            // Encode Unicode codepoint to UTF-8
            char buf[5] = {};
            uint32_t cp = event.character;
            if (cp < 0x80)
            {
                buf[0] = static_cast<char>(cp);
            }
            else if (cp < 0x800)
            {
                buf[0] = static_cast<char>(0xC0 | (cp >> 6));
                buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                buf[0] = static_cast<char>(0xE0 | (cp >> 12));
                buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                buf[0] = static_cast<char>(0xF0 | (cp >> 18));
                buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                buf[2] = static_cast<char>(0x80 | ((cp >> 6)  & 0x3F));
                buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
            }
            controller->insertText(std::string_view(buf));
            if (on_changed) on_changed(controller->text());
            resetCursorBlink();
            return true;
        }

        return false;
    }

    // -------------------------------------------------------------------------
    // Pointer
    // -------------------------------------------------------------------------

    void RenderTextField::onPointerEvent(const PointerEvent& event)
    {
        if (event.kind == PointerEventKind::down)
        {
            pressed_ = true;
            pointer_id_ = event.pointer_id;

            // Capture the pointer so parent scroll views don't scroll
            if (auto* d = PointerDispatcher::activeDispatcher())
                d->capturePointer(pointer_id_, this);

            if (on_tap) on_tap(); // request focus from owning State
            if (controller)
            {
                float local_x = event.position.x - global_offset_.x - padding_h;
                float local_y = event.position.y - global_offset_.y - padding_v;
                int idx = hitTestText(local_x, local_y);
                controller->setSelection(idx, idx);
            }
            resetCursorBlink();
        }
        else if (event.kind == PointerEventKind::move && pressed_ && controller)
        {
            // Only handle move events for the pointer that pressed this field
            if (event.pointer_id != pointer_id_)
                return;
            float local_x = event.position.x - global_offset_.x - padding_h;
            float local_y = event.position.y - global_offset_.y - padding_v;
            int idx = hitTestText(local_x, local_y);
            // Extend selection keeping start fixed
            controller->setSelection(controller->selectionStart(), idx);
            resetCursorBlink();
        }
        else if (event.kind == PointerEventKind::up ||
                 event.kind == PointerEventKind::cancel)
        {
            // Only handle up/cancel for the pointer that pressed this field
            if (pressed_ && event.pointer_id != pointer_id_)
                return;

            // Release the pointer capture
            if (auto* d = PointerDispatcher::activeDispatcher())
                d->releasePointer(pointer_id_);

            pressed_ = false;
            pointer_id_ = 0;
        }
        else if (event.kind == PointerEventKind::scroll)
        {
            if (!isMultiline()) return;

            // Handle scroll wheel/trackpad scroll events
            applyScrollDelta(event.scroll_delta_y);
        }
    }

    // -------------------------------------------------------------------------
    // Scrolling
    // -------------------------------------------------------------------------

    void RenderTextField::applyScrollDelta(float delta)
    {
        // Calculate the maximum scroll offset based on content height
        float content_height = getLineCount() * lineHeight();
        float viewport_height = size_.height - padding_v * 2.0f;
        float max_scroll = std::max(0.0f, content_height - viewport_height);

        // Apply the delta and clamp
        scroll_offset_y_ = std::clamp(scroll_offset_y_ + delta, 0.0f, max_scroll);
        markNeedsPaint();
    }

    // -------------------------------------------------------------------------
    // Tick (cursor blink)
    // -------------------------------------------------------------------------

    void RenderTextField::onTick(uint64_t now_ms)
    {
        if (!focused) return;
        if (now_ms - last_blink_ms_ >= kBlinkPeriodMs)
        {
            cursor_visible_ = !cursor_visible_;
            last_blink_ms_  = now_ms;
            markNeedsPaint();
        }
    }

    void RenderTextField::resetCursorBlink()
    {
        cursor_visible_ = true;
        last_blink_ms_  = 0; // will reset on next tick
        markNeedsPaint();
    }

    // -------------------------------------------------------------------------
    // Text measurement helpers
    // -------------------------------------------------------------------------

    float RenderTextField::measurePrefix(int byte_end) const
    {
        if (!controller || byte_end <= 0) return 0.0f;
        std::string disp = displayText();
        int clamped = std::min(byte_end, static_cast<int>(disp.size()));
        std::string prefix = disp.substr(0, static_cast<size_t>(clamped));

        return measureText(prefix);
    }

    float RenderTextField::measureText(const std::string& text) const
    {
        if (text.empty()) return 0.0f;

        if (IDrawBackend* backend = RenderObject::activeBackend())
            return backend->measureText(TextSpan{text, style}).width;

        return static_cast<float>(text.size()) * style.font_size * 0.6f;
    }

    float RenderTextField::textHeight() const
    {
        if (IDrawBackend* backend = RenderObject::activeBackend())
        {
            Size sz = backend->measureText(TextSpan{"M", style});
            return sz.height;
        }
        return style.font_size * 1.2f;
    }

    float RenderTextField::lineHeight() const
    {
        return textHeight() * 1.2f;
    }

    // -------------------------------------------------------------------------
    // Multi-line helpers
    // -------------------------------------------------------------------------

    int RenderTextField::getLineCount() const
    {
        if (!lines_.empty()) return static_cast<int>(lines_.size());

        std::string text = controller ? controller->text() : "";
        if (text.empty())
        {
            lines_.push_back("");
            return 1;
        }

        // Simple word wrapping
        float max_width = constraints_.max_width - padding_h * 2.0f;
        if (max_width < 0) max_width = 0;

        std::istringstream stream(text);
        std::string line;

        while (std::getline(stream, line, '\n'))
        {
            // Check if line needs wrapping
            float line_width = measureText(line);
            if (line_width <= max_width || max_width <= 0)
            {
                lines_.push_back(line);
            }
            else
            {
                // Wrap the line
                std::string current;
                for (char c : line)
                {
                    std::string test = current + c;
                    if (measureText(test) > max_width && !current.empty())
                    {
                        lines_.push_back(current);
                        current = c;
                    }
                    else
                    {
                        current = test;
                    }
                }
                if (!current.empty())
                {
                    lines_.push_back(current);
                }
            }
        }

        if (lines_.empty())
        {
            lines_.push_back("");
        }

        return static_cast<int>(lines_.size());
    }

    int RenderTextField::getLineForPosition(int byte_pos) const
    {
        getLineCount();

        int current_pos = 0;
        for (size_t i = 0; i < lines_.size(); ++i)
        {
            int line_len = static_cast<int>(lines_[i].size());
            if (byte_pos >= current_pos && byte_pos <= current_pos + line_len)
            {
                return static_cast<int>(i);
            }
            current_pos += line_len + 1; // +1 for newline
        }
        return static_cast<int>(lines_.size()) - 1;
    }

    int RenderTextField::getPositionForLine(int line) const
    {
        getLineCount();

        int pos = 0;
        for (int i = 0; i < line && i < static_cast<int>(lines_.size()); ++i)
        {
            pos += static_cast<int>(lines_[i].size()) + 1; // +1 for newline
        }
        return pos;
    }

    float RenderTextField::getLineWidth(int line) const
    {
        getLineCount();
        if (line < 0 || line >= static_cast<int>(lines_.size())) return 0.0f;
        return measureText(lines_[line]);
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------

    int RenderTextField::hitTestText(float local_x, float local_y) const
    {
        if (isMultiline())
        {
            // Find which line was clicked
            float line_h = lineHeight();
            int line = static_cast<int>((local_y + scroll_offset_y_) / line_h);
            line = std::max(0, std::min(line, getLineCount() - 1));

            // Get character position within that line
            int line_start = getPositionForLine(line);
            const std::string& line_text = lines_[line];

            int best_idx = 0;
            float best_dist = std::abs(local_x - 0.0f);

            for (size_t i = 1; i <= line_text.size(); ++i)
            {
                float x = measureText(line_text.substr(0, i));
                float dist = std::abs(local_x - x);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_idx = static_cast<int>(i);
                }
            }

            return line_start + best_idx;
        }
        else
        {
            return hitTestSingleLine(local_x);
        }
    }

    int RenderTextField::hitTestSingleLine(float local_x) const
    {
        if (!controller) return 0;
        const std::string disp = displayText();
        const int len = static_cast<int>(disp.size());
        if (len == 0 || local_x <= 0.0f) return 0;

        float best_dist = std::abs(local_x - 0.0f);
        int   best_idx  = 0;

        for (int i = 1; i <= len; ++i)
        {
            float x   = measurePrefix(i);
            float dist = std::abs(local_x - x);
            if (dist < best_dist)
            {
                best_dist = dist;
                best_idx  = i;
            }
        }
        return best_idx;
    }

    // -------------------------------------------------------------------------
    // IME geometry queries
    // -------------------------------------------------------------------------

    Rect RenderTextField::getRectForCharacterRange(int start, int end) const
    {
        if (!controller) return Rect::zero();

        const float line_h = lineHeight();
        const std::string disp = displayText();
        int sz = static_cast<int>(disp.size());
        start = std::clamp(start, 0, sz);
        end   = std::clamp(end,   0, sz);

        if (isMultiline())
        {
            getLineCount();
            int line = getLineForPosition(start);
            int line_start = getPositionForLine(line);
            const std::string& line_text = lines_[line];

            int offset_in_line = start - line_start;
            int end_in_line    = std::min(end - line_start, static_cast<int>(line_text.size()));

            float x0 = padding_h + measureText(line_text.substr(0, static_cast<size_t>(offset_in_line)));
            float x1 = padding_h + measureText(line_text.substr(0, static_cast<size_t>(end_in_line)));
            float y  = padding_v + line * line_h - scroll_offset_y_;

            return Rect::fromLTWH(x0, y, x1 - x0, line_h);
        }
        else
        {
            float x0 = padding_h + measurePrefix(start);
            float x1 = padding_h + measurePrefix(end);
            float y  = padding_v;
            return Rect::fromLTWH(x0, y, x1 - x0, line_h);
        }
    }

    int RenderTextField::getPositionForPoint(float local_x, float local_y) const
    {
        return hitTestText(local_x, local_y);
    }

    // -------------------------------------------------------------------------
    // Display helpers
    // -------------------------------------------------------------------------

    std::string RenderTextField::displayText() const
    {
        if (!controller) return {};
        if (!obscure_text) return controller->text();

        // Replace each character with a bullet (•, U+2022 = UTF-8: E2 80 A2)
        std::string result;
        result.reserve(controller->text().size() * 3);
        const char bullet[] = "\xE2\x80\xA2";
        for (size_t i = 0; i < controller->text().size(); ++i)
            result += bullet;
        return result;
    }

} // namespace systems::leal::campello_widgets
