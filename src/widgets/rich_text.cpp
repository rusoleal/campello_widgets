#include <campello_widgets/widgets/rich_text.hpp>
#include <campello_widgets/ui/render_paragraph.hpp>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // Forward declaration for the render object widget
    class RichTextRenderObjectWidget : public RenderObjectWidget
    {
    public:
        std::shared_ptr<InlineSpan> text;
        TextStyle default_style;
        TextAlign text_align;
        size_t max_lines;
        bool soft_wrap;

        RichTextRenderObjectWidget(
            std::shared_ptr<InlineSpan> t,
            TextStyle style,
            TextAlign align,
            size_t lines,
            bool wrap)
            : text(std::move(t))
            , default_style(std::move(style))
            , text_align(align)
            , max_lines(lines)
            , soft_wrap(wrap)
        {}

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto render_object = std::make_shared<RenderParagraph>();
            render_object->setText(text);
            render_object->setDefaultStyle(default_style);
            render_object->setTextAlign(text_align);
            render_object->setMaxLines(max_lines);
            render_object->setSoftWrap(soft_wrap);
            return render_object;
        }

        void updateRenderObject(RenderObject& obj) const override
        {
            auto& paragraph = static_cast<RenderParagraph&>(obj);
            paragraph.setText(text);
            paragraph.setDefaultStyle(default_style);
            paragraph.setTextAlign(text_align);
            paragraph.setMaxLines(max_lines);
            paragraph.setSoftWrap(soft_wrap);
        }
    };

    WidgetRef RichText::build(BuildContext& context) const
    {
        (void)context;

        // Apply default style to the text tree
        std::shared_ptr<InlineSpan> styled_text;
        if (text) {
            styled_text = text->applyStyle(default_style);
        } else {
            styled_text = std::make_shared<InlineTextSpan>("");
        }

        return std::make_shared<RichTextRenderObjectWidget>(
            styled_text,
            default_style,
            text_align,
            max_lines,
            soft_wrap);
    }

} // namespace systems::leal::campello_widgets
