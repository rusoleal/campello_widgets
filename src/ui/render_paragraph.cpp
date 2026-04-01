#include <campello_widgets/ui/render_paragraph.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/text_span.hpp>

#include <limits>
#include <sstream>

namespace systems::leal::campello_widgets
{

    void RenderParagraph::setText(std::shared_ptr<InlineSpan> text)
    {
        if (text_ != text) {
            text_ = std::move(text);
            markNeedsLayout();
        }
    }

    void RenderParagraph::setDefaultStyle(const TextStyle& style) noexcept
    {
        if (!(default_style_ == style)) {
            default_style_ = style;
            markNeedsLayout();
        }
    }

    void RenderParagraph::setTextAlign(TextAlign align) noexcept
    {
        if (text_align_ != align) {
            text_align_ = align;
            markNeedsLayout();
        }
    }

    void RenderParagraph::setMaxLines(size_t lines) noexcept
    {
        if (max_lines_ != lines) {
            max_lines_ = lines;
            markNeedsLayout();
        }
    }

    void RenderParagraph::setSoftWrap(bool wrap) noexcept
    {
        if (soft_wrap_ != wrap) {
            soft_wrap_ = wrap;
            markNeedsLayout();
        }
    }

    void RenderParagraph::collectRuns()
    {
        runs_.clear();
        if (text_) {
            text_->collectTextRuns(runs_);
        }
    }

    float RenderParagraph::measureText(const std::string& text, const TextStyle& style) const
    {
        if (text.empty()) return 0.0f;

        if (IDrawBackend* backend = RenderObject::activeBackend()) {
            return backend->measureText(TextSpan{text, style}).width;
        }

        // Fallback estimation
        const float char_width = style.font_size * 0.6f;
        return char_width * static_cast<float>(text.size());
    }

    float RenderParagraph::measureHeight(const TextStyle& style) const
    {
        // Line height is typically 1.2x font size
        return style.font_size * 1.2f;
    }

    size_t RenderParagraph::findWordBreak(const std::string& text, size_t start) const
    {
        if (start >= text.size()) return text.size();

        // Find end of current word
        size_t pos = start;
        while (pos < text.size() && text[pos] != ' ' && text[pos] != '\t') {
            ++pos;
        }

        // Include trailing spaces
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
            ++pos;
        }

        return pos;
    }

    void RenderParagraph::computeLines()
    {
        lines_.clear();
        max_line_width_ = 0.0f;

        if (runs_.empty()) {
            return;
        }

        const float max_width = soft_wrap_ ? constraints_.max_width : std::numeric_limits<float>::max();
        const bool has_max_width = max_width != std::numeric_limits<float>::max() && max_width > 0.0f;

        Line current_line;
        float current_line_width = 0.0f;
        float current_line_height = 0.0f;

        auto flushLine = [&]() {
            if (!current_line.spans.empty()) {
                current_line.width = current_line_width;
                current_line.height = current_line_height;
                lines_.push_back(std::move(current_line));
                current_line = Line{};
                current_line_width = 0.0f;
                current_line_height = 0.0f;
            }
        };

        auto addSpanToLine = [&](const std::string& text, const TextStyle& style, float width) {
            LineSpan span;
            span.text = text;
            span.style = style;
            span.x_offset = current_line_width;
            span.width = width;
            current_line.spans.push_back(std::move(span));
            current_line_width += width;
            current_line_height = std::max(current_line_height, measureHeight(style));
        };

        for (const auto& run : runs_) {
            if (run.text.empty()) continue;

            std::string remaining = run.text;
            TextStyle style = run.style;

            while (!remaining.empty()) {
                // Check if we need to wrap
                if (has_max_width && current_line_width > 0.0f) {
                    float remaining_width = measureText(remaining, style);
                    if (current_line_width + remaining_width > max_width) {
                        // Need to wrap - flush current line
                        flushLine();
                    }
                }

                if (!has_max_width || current_line_width == 0.0f) {
                    // Starting a new line or no wrapping needed
                    // Find how much text fits
                    size_t pos = 0;
                    float accumulated_width = 0.0f;

                    while (pos < remaining.size()) {
                        size_t next_break = findWordBreak(remaining, pos);
                        std::string word = remaining.substr(pos, next_break - pos);
                        float word_width = measureText(word, style);

                        if (has_max_width && current_line_width + accumulated_width + word_width > max_width) {
                            // This word doesn't fit
                            if (pos == 0 && current_line_width == 0.0f) {
                                // First word is too long - take as much as possible
                                // For now, just take the whole word (character breaking would be better)
                                accumulated_width = word_width;
                                pos = next_break;
                            }
                            break;
                        }

                        accumulated_width += word_width;
                        pos = next_break;
                    }

                    if (pos > 0) {
                        std::string text_to_add = remaining.substr(0, pos);
                        addSpanToLine(text_to_add, style, accumulated_width);
                        remaining = remaining.substr(pos);
                    } else {
                        // Couldn't fit anything - force break
                        addSpanToLine(remaining, style, measureText(remaining, style));
                        remaining.clear();
                    }

                    // Check if we should wrap after this
                    if (has_max_width && current_line_width >= max_width * 0.9f) {
                        flushLine();
                    }
                } else {
                    // Add remaining text to current line
                    float width = measureText(remaining, style);
                    addSpanToLine(remaining, style, width);
                    remaining.clear();
                }

                // Check max lines
                if (max_lines_ > 0 && lines_.size() >= max_lines_) {
                    // Truncate remaining content
                    if (!lines_.empty() && !lines_.back().spans.empty()) {
                        // Add ellipsis to last span
                        auto& last_span = lines_.back().spans.back();
                        last_span.text += "...";
                        last_span.width = measureText(last_span.text, last_span.style);
                        lines_.back().width = 0.0f;
                        for (const auto& s : lines_.back().spans) {
                            lines_.back().width += s.width;
                        }
                    }
                    return;
                }
            }
        }

        flushLine();

        // Compute max line width
        for (const auto& line : lines_) {
            max_line_width_ = std::max(max_line_width_, line.width);
        }

        // Apply text alignment to compute x offsets
        for (auto& line : lines_) {
            if (line.width >= max_width * 0.99f) {
                continue;  // Line fills width, no alignment needed
            }

            float offset = 0.0f;
            switch (text_align_) {
                case TextAlign::left:
                case TextAlign::start:
                    offset = 0.0f;
                    break;
                case TextAlign::right:
                case TextAlign::end:
                    offset = max_width - line.width;
                    break;
                case TextAlign::center:
                    offset = (max_width - line.width) * 0.5f;
                    break;
                case TextAlign::justify:
                    // Not implemented yet
                    offset = 0.0f;
                    break;
            }

            for (auto& span : line.spans) {
                span.x_offset += offset;
            }
        }
    }

    void RenderParagraph::performLayout()
    {
        collectRuns();
        computeLines();

        // Compute total height
        float total_height = 0.0f;
        for (const auto& line : lines_) {
            total_height += line.height;
        }

        size_ = constraints_.constrain(Size{max_line_width_, total_height});
    }

    void RenderParagraph::performPaint(PaintContext& context, const Offset& offset)
    {
        // Scale font size by DPR for physical pixel rasterization.
        const float dpr = activeDevicePixelRatio();

        float y_offset = 0.0f;

        for (const auto& line : lines_) {
            for (const auto& span : line.spans) {
                Offset span_offset{
                    offset.x + span.x_offset,
                    offset.y + y_offset
                };
                TextStyle scaled_style = span.style;
                scaled_style.font_size *= dpr;
                context.canvas().drawText(TextSpan{span.text, scaled_style}, span_offset);
            }
            y_offset += line.height;
        }
    }

} // namespace systems::leal::campello_widgets
