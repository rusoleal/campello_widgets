#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/inline_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that lays out and paints multi-style text (RichText).
     *
     * RenderParagraph takes an InlineSpan tree, flattens it into styled text runs,
     * performs word wrapping across runs, and paints each run with its associated
     * style. This enables rich text with mixed colors, font sizes, and weights.
     */
    class RenderParagraph : public RenderBox
    {
    public:
        RenderParagraph() = default;

        void setText(std::shared_ptr<InlineSpan> text);
        void setDefaultStyle(const TextStyle& style) noexcept;
        void setTextAlign(TextAlign align) noexcept;
        void setMaxLines(size_t lines) noexcept;
        void setSoftWrap(bool wrap) noexcept;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        /** @brief Information about a line of text for layout and painting. */
        struct LineSpan
        {
            std::string text;
            TextStyle   style;
            float       x_offset = 0.0f;  // Position within the line
            float       width = 0.0f;     // Measured width
        };

        struct Line
        {
            std::vector<LineSpan> spans;
            float                 height = 0.0f;
            float                 width = 0.0f;
        };

        /** @brief Collects all text runs from the span tree. */
        void collectRuns();

        /** @brief Performs word wrapping to create lines. */
        void computeLines();

        /** @brief Measures text width using backend or estimation. */
        float measureText(const std::string& text, const TextStyle& style) const;

        /** @brief Measures text height for a given style. */
        float measureHeight(const TextStyle& style) const;

        /** @brief Finds the next word break position. */
        size_t findWordBreak(const std::string& text, size_t start) const;

        std::shared_ptr<InlineSpan> text_;
        TextStyle                   default_style_;
        TextAlign                   text_align_ = TextAlign::start;
        size_t                      max_lines_ = 0;  // 0 = unlimited
        bool                        soft_wrap_ = true;

        // Cached layout data
        std::vector<InlineSpan::TextRun> runs_;
        std::vector<Line>                lines_;
        float                            max_line_width_ = 0.0f;
    };

} // namespace systems::leal::campello_widgets
