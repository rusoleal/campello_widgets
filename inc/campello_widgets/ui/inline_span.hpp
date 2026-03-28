#pragma once

#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/size.hpp>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class IDrawBackend;
    class RenderObject;

    /**
     * @brief Abstract base class for inline text spans.
     *
     * InlineSpan is the base class for all inline text content in a RichText widget.
     * It forms a tree structure where leaf nodes are TextSpans with actual text,
     * and future extensions could include WidgetSpans (embedding widgets in text).
     *
     * The span tree is laid out as a paragraph with word wrapping and mixed styles.
     */
    class InlineSpan
    {
    public:
        virtual ~InlineSpan() = default;

        /**
         * @brief Returns true if this is a leaf span (TextSpan with actual text).
         */
        virtual bool isLeaf() const noexcept { return false; }

        /**
         * @brief Returns the text content if this is a leaf, empty otherwise.
         */
        virtual std::string text() const { return ""; }

        /**
         * @brief Returns the style applied to this span.
         */
        virtual TextStyle style() const { return {}; }

        /**
         * @brief Returns child spans. Empty for leaf spans.
         */
        virtual const std::vector<std::shared_ptr<InlineSpan>>& children() const
        {
            static const std::vector<std::shared_ptr<InlineSpan>> empty;
            return empty;
        }

        /**
         * @brief Flattens this span into a list of styled text runs.
         *
         * This recursively traverses the span tree, producing a linear sequence
         * of text runs with their associated styles. This is used for layout
         * and painting.
         */
        struct TextRun
        {
            std::string text;
            TextStyle   style;
        };

        virtual void collectTextRuns(std::vector<TextRun>& runs) const = 0;

        /**
         * @brief Computes the intrinsic width of this span.
         */
        virtual float computeIntrinsicWidth(IDrawBackend* backend) const = 0;

        /**
         * @brief Creates a copy of this span with the given style merged in.
         */
        virtual std::shared_ptr<InlineSpan> applyStyle(const TextStyle& parent_style) const = 0;
    };

    /**
     * @brief A leaf text span with a string and style.
     *
     * InlineTextSpan is the atomic unit of rich text. It contains the actual
     * text content and the style to apply when rendering.
     */
    class InlineTextSpan : public InlineSpan
    {
    public:
        std::string text_content;
        TextStyle   text_style;

        InlineTextSpan() = default;
        InlineTextSpan(std::string text, TextStyle style = {})
            : text_content(std::move(text))
            , text_style(std::move(style))
        {}

        bool isLeaf() const noexcept override { return true; }
        std::string text() const override { return text_content; }
        TextStyle style() const override { return text_style; }

        void collectTextRuns(std::vector<TextRun>& runs) const override
        {
            if (!text_content.empty()) {
                runs.push_back({text_content, text_style});
            }
        }

        float computeIntrinsicWidth(IDrawBackend* backend) const override;

        std::shared_ptr<InlineSpan> applyStyle(const TextStyle& parent_style) const override
        {
            auto merged = std::make_shared<InlineTextSpan>();
            merged->text_content = text_content;
            merged->text_style = parent_style.merge(text_style);
            return merged;
        }

        // Factory methods for convenience
        static std::shared_ptr<InlineTextSpan> create(std::string text, TextStyle style = {})
        {
            return std::make_shared<InlineTextSpan>(std::move(text), std::move(style));
        }
    };

    /**
     * @brief A composite span that groups multiple child spans.
     *
     * CompositeInlineSpan holds multiple child spans sequentially, making it
     * easy to create rich text with multiple styles. It has no styling of its
     * own; it just forwards to its children.
     */
    class CompositeInlineSpan : public InlineSpan
    {
    public:
        std::vector<std::shared_ptr<InlineSpan>> child_spans;

        CompositeInlineSpan() = default;
        explicit CompositeInlineSpan(std::vector<std::shared_ptr<InlineSpan>> children)
            : child_spans(std::move(children))
        {}

        const std::vector<std::shared_ptr<InlineSpan>>& children() const override
        {
            return child_spans;
        }

        void collectTextRuns(std::vector<TextRun>& runs) const override
        {
            for (const auto& child : child_spans) {
                if (child) child->collectTextRuns(runs);
            }
        }

        float computeIntrinsicWidth(IDrawBackend* backend) const override
        {
            float width = 0.0f;
            for (const auto& child : child_spans) {
                if (child) width += child->computeIntrinsicWidth(backend);
            }
            return width;
        }

        std::shared_ptr<InlineSpan> applyStyle(const TextStyle& parent_style) const override
        {
            auto merged = std::make_shared<CompositeInlineSpan>();
            merged->child_spans.reserve(child_spans.size());
            for (const auto& child : child_spans) {
                if (child) {
                    merged->child_spans.push_back(child->applyStyle(parent_style));
                }
            }
            return merged;
        }
    };

} // namespace systems::leal::campello_widgets
