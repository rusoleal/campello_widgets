#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/inline_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <memory>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Displays text that uses multiple different styles.
     *
     * RichText displays text with multiple styles, allowing for inline formatting
     * such as bold, italic, color changes, and font changes within a single
     * paragraph.
     *
     * Example usage:
     * @code
     * RichText::create({
     *     InlineTextSpan::create("Hello ", TextStyle{}.fontSize(16)),
     *     InlineTextSpan::create("bold", TextStyle{}.bold().fontSize(16)),
     *     InlineTextSpan::create(" and ", TextStyle{}.fontSize(16)),
     *     InlineTextSpan::create("colored", TextStyle{}.color(Colors::red).fontSize(16)),
     *     InlineTextSpan::create(" text!", TextStyle{}.fontSize(16)),
     * })
     * @endcode
     *
     * @see InlineTextSpan for the individual text runs
     * @see Text for a simpler single-style text widget
     */
    class RichText : public StatelessWidget
    {
    public:
        /** @brief The root span containing all text runs. */
        std::shared_ptr<InlineSpan> text;

        /** @brief Default text style inherited by all spans. */
        TextStyle default_style;

        /** @brief How to align the text horizontally. */
        TextAlign text_align = TextAlign::start;

        /** @brief Maximum number of lines. 0 means unlimited. */
        size_t max_lines = 0;

        /** @brief Whether to soft-wrap text at the edge. */
        bool soft_wrap = true;

        RichText() = default;

        explicit RichText(std::shared_ptr<InlineSpan> root_span)
            : text(std::move(root_span))
        {}

        RichText(std::vector<std::shared_ptr<InlineSpan>> spans, TextStyle style = {})
            : default_style(std::move(style))
        {
            // Create a composite span from the list
            text = std::make_shared<CompositeInlineSpan>(std::move(spans));
        }

        WidgetRef build(BuildContext& context) const override;

        // Convenience factory methods

        /** @brief Creates RichText from a list of inline spans. */
        static std::shared_ptr<RichText> create(
            std::vector<std::shared_ptr<InlineSpan>> spans,
            TextStyle default_style = {})
        {
            return std::make_shared<RichText>(std::move(spans), std::move(default_style));
        }

        /** @brief Creates RichText from a single root span. */
        static std::shared_ptr<RichText> create(std::shared_ptr<InlineSpan> root_span)
        {
            auto rt = std::make_shared<RichText>();
            rt->text = std::move(root_span);
            return rt;
        }
    };

} // namespace systems::leal::campello_widgets
