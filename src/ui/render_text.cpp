#include <campello_widgets/ui/render_text.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/paint_context.hpp>

#include <limits>
#include <sstream>

namespace systems::leal::campello_widgets
{

    void RenderText::setTextSpan(TextSpan span) noexcept
    {
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
        size_ = constraints_.constrain(Size{max_line_width, total_height});
    }

    void RenderText::performPaint(PaintContext& context, const Offset& offset)
    {
        // Scale font size by DPR for physical pixel rasterization.
        // Layout is done in logical pixels; only the final paint scales text.
        const float dpr = activeDevicePixelRatio();
        TextStyle scaled_style = span_.style;
        scaled_style.font_size *= dpr;

        for (std::size_t i = 0; i < lines_.size(); ++i) {
            const Offset line_offset{offset.x, offset.y + line_height_ * static_cast<float>(i)};
            context.canvas().drawText(TextSpan{lines_[i], scaled_style}, line_offset);
        }
    }

} // namespace systems::leal::campello_widgets
