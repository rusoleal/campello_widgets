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

namespace systems::leal::campello_widgets
{

    static constexpr uint64_t kBlinkPeriodMs = 530;
    static constexpr float    kCursorWidth   = 2.0f;

    // -------------------------------------------------------------------------

    RenderTextField::RenderTextField()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this,     [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now_ms)        { onTick(now_ms);   });
        }
    }

    RenderTextField::~RenderTextField()
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
        // Height: at least min_height, at most constrained max
        float line_height = style.font_size * 1.2f;
        float natural_h   = std::max(min_height, line_height + padding_v * 2.0f);

        float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
        float h = std::isinf(constraints_.max_height) ? natural_h
                                                       : std::min(natural_h, constraints_.max_height);
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

        // Vertical centre for text baseline
        float line_height = style.font_size * 1.2f;
        float ty = iy + (ih - line_height) * 0.5f;

        if (empty && !placeholder.empty())
        {
            // Placeholder
            TextStyle ph_style = style;
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
                    Rect::fromLTWH(sel_x0, ty, sel_x1 - sel_x0, line_height),
                    Paint::filled(selection_color));
            }

            // Text
            canvas.drawText(TextSpan{disp, style}, {ix, ty});
        }

        // Cursor
        if (focused && cursor_visible_ && controller)
        {
            int cursor_pos = controller->selectionEnd();
            float cx = ix + measurePrefix(cursor_pos);
            canvas.drawRect(
                Rect::fromLTWH(cx, ty, kCursorWidth, line_height),
                Paint::filled(cursor_color));
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
            if (on_submitted) on_submitted(controller->text());
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

        case KeyCode::home:
            controller->setSelection(shift ? controller->selectionStart() : 0, 0);
            resetCursorBlink();
            return true;

        case KeyCode::end:
        {
            int end = static_cast<int>(controller->text().size());
            controller->setSelection(shift ? controller->selectionStart() : end, end);
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
            if (on_tap) on_tap(); // request focus from owning State
            if (controller)
            {
                float local_x = event.position.x - global_offset_.x - padding_h;
                int idx = hitTestText(local_x);
                controller->setSelection(idx, idx);
            }
            resetCursorBlink();
        }
        else if (event.kind == PointerEventKind::move && pressed_ && controller)
        {
            float local_x = event.position.x - global_offset_.x - padding_h;
            int idx = hitTestText(local_x);
            // Extend selection keeping start fixed
            controller->setSelection(controller->selectionStart(), idx);
            resetCursorBlink();
        }
        else if (event.kind == PointerEventKind::up ||
                 event.kind == PointerEventKind::cancel)
        {
            pressed_ = false;
        }
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

        if (IDrawBackend* backend = RenderObject::activeBackend())
            return backend->measureText(TextSpan{prefix, style}).width;

        return static_cast<float>(clamped) * style.font_size * 0.6f;
    }

    int RenderTextField::hitTestText(float local_x) const
    {
        if (!controller) return 0;
        const std::string disp = displayText();
        const int len = static_cast<int>(disp.size());
        if (len == 0 || local_x <= 0.0f) return 0;

        // Binary search would be better; linear is fine for single-line fields
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
