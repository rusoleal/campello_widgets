#pragma once

#include <string>
#include <vector>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/text_span.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that measures and paints a single TextSpan.
     *
     * Text measurement in `performLayout()` uses a stub estimation
     * (character_width ≈ font_size × 0.6, line_height ≈ font_size × 1.2).
     * Real font metrics will be wired up when the GPU font atlas is
     * implemented in the text rendering phase.
     *
     * Word wrapping is performed during layout: words are broken across lines
     * so that no line exceeds `constraints_.max_width`.
     */
    class RenderText : public RenderBox
    {
    public:
        void setTextSpan(TextSpan span) noexcept;
        const TextSpan& textSpan() const noexcept { return span_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void debugPaint(PaintContext& context, const Offset& offset) const override;

    private:
        /** @brief Estimates the bounding size of a single-line span. */
        Size measureText() const noexcept;

        /** @brief Breaks `span_.text` into word-wrapped lines within `max_width`. */
        void computeLines(float max_width);

        TextSpan             span_;
        std::vector<std::string> lines_;
        float                line_height_ = 0.0f;
    };

} // namespace systems::leal::campello_widgets
