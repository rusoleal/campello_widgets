#include <campello_widgets/ui/render_text.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/paint.hpp>

#include <limits>
#include <sstream>

namespace systems::leal::campello_widgets
{

    void RenderText::setTextSpan(TextSpan span) noexcept
    {
        if (span_ == span) return;
        span_ = std::move(span);
        markNeedsLayout();
        markNeedsPaint();
    }

    Size RenderText::measureText() const noexcept
    {
        if (IDrawBackend* backend = RenderObject::activeBackend())
            return backend->measureText(span_);

        // Fallback stub when no backend is registered yet.
        const float char_width  = span_.style.font_size * 0.6f;
        const float line_height = span_.style.font_size * 1.2f;
        const float text_width  = char_width * static_cast<float>(span_.text.size());
        return Size{text_width, line_height};
    }

    void RenderText::computeLines(float max_width)
    {
        lines_.clear();

        const float char_width = span_.style.font_size * 0.6f;

        auto measureWord = [&](const std::string& word) -> float {
            if (IDrawBackend* backend = RenderObject::activeBackend())
                return backend->measureText(TextSpan{word, span_.style}).width;
            return char_width * static_cast<float>(word.size());
        };

        // No horizontal constraint — single line.
        if (max_width == std::numeric_limits<float>::infinity() || max_width <= 0.0f) {
            lines_.push_back(span_.text);
            return;
        }

        // Split into words and greedily pack into lines.
        std::istringstream stream(span_.text);
        std::string word;
        std::string current_line;

        while (stream >> word) {
            if (current_line.empty()) {
                current_line = word;
            } else {
                const std::string candidate       = current_line + ' ' + word;
                const float       candidate_width = measureWord(candidate);
                if (candidate_width <= max_width) {
                    current_line = candidate;
                } else {
                    lines_.push_back(std::move(current_line));
                    current_line = word;
                }
            }
        }

        if (!current_line.empty())
            lines_.push_back(std::move(current_line));

        if (lines_.empty())
            lines_.push_back("");
    }

    void RenderText::performLayout()
    {
        line_height_ = measureText().height;


        computeLines(constraints_.max_width);

        float max_line_width = 0.0f;
        for (const auto& line : lines_) {
            float w;
            if (IDrawBackend* backend = RenderObject::activeBackend())
                w = backend->measureText(TextSpan{line, span_.style}).width;
            else
                w = span_.style.font_size * 0.6f * static_cast<float>(line.size());
            if (w > max_line_width)
                max_line_width = w;
        }

        const float total_height = line_height_ * static_cast<float>(lines_.size());

        // Only meaningful (and only implemented) for a single computed
        // line — see TextStyle::tight_vertical_bounds's doc for why this
        // doesn't apply to multi-line text.
        tight_bounds_active_ = span_.style.tight_vertical_bounds && lines_.size() == 1;
        if (tight_bounds_active_)
        {
            IDrawBackend* backend = RenderObject::activeBackend();
            // Query with the same dpr-scaled font_size performPaint() will
            // actually rasterize at, then convert the result back to
            // logical pixels — see measureTextInkBounds()'s doc for why
            // this has to mirror rasterizeText()'s physical-pixel rounding
            // rather than working in logical units throughout.
            const float dpr = activeDevicePixelRatio();
            TextStyle scaled_style = span_.style;
            scaled_style.font_size *= dpr;
            const Rect ink_px = backend
                ? backend->measureTextInkBounds(TextSpan{lines_[0], scaled_style})
                : Rect{0.0f, 0.0f, max_line_width * dpr, line_height_ * dpr};
            tight_bounds_top_offset_ = ink_px.y / dpr;
            size_ = constraints_.constrain(Size{max_line_width, ink_px.height / dpr});
        }
        else
        {
            size_ = constraints_.constrain(Size{max_line_width, total_height});
        }
    }

    void RenderText::performPaint(PaintContext& context, const Offset& offset)
    {
        // Scale font size by DPR for physical pixel rasterization.
        // Layout is done in logical pixels; only the final paint scales text.
        const float dpr = activeDevicePixelRatio();
        TextStyle scaled_style = span_.style;
        scaled_style.font_size *= dpr;

        if (tight_bounds_active_)
        {
            // The rendered texture is still the full typographic box
            // (rasterizeText() is unchanged) — shift it up so the ink,
            // not the typographic top, lands at `offset` (this class's
            // own reported size_ now tightly wraps just the ink; see
            // performLayout()).
            const Offset line_offset{offset.x, offset.y - tight_bounds_top_offset_};
            context.canvas().drawText(TextSpan{lines_[0], scaled_style}, line_offset);
            return;
        }

        for (std::size_t i = 0; i < lines_.size(); ++i) {
            const Offset line_offset{offset.x, offset.y + line_height_ * static_cast<float>(i)};
            context.canvas().drawText(TextSpan{lines_[i], scaled_style}, line_offset);
        }
    }

    std::optional<float> RenderText::computeDistanceToActualBaseline(TextBaseline) const
    {
        if (lines_.empty()) return std::nullopt;
        // Approximate alphabetic/ideographic baseline: roughly 75% down from
        // the top of the font's em-box -- same approximation debugPaint()
        // already used for its debug baseline lines, now promoted to a real
        // measurement other render objects (e.g. RenderBaseline) can use.
        return span_.style.font_size * 0.75f;
    }

    void RenderText::debugPaint(PaintContext& context, const Offset& offset) const
    {
        if (!DebugFlags::paintBaselinesEnabled) return;

        const float baseline_offset = span_.style.font_size * 0.75f;

        for (std::size_t i = 0; i < lines_.size(); ++i) {
            const float y = offset.y + line_height_ * static_cast<float>(i) + baseline_offset;
            context.canvas().drawLine(
                Offset{offset.x, y},
                Offset{offset.x + size_.width, y},
                Paint::stroked(Color::fromRGB(0.0f, 0.8f, 0.0f), 1.0f));
        }
    }

} // namespace systems::leal::campello_widgets
