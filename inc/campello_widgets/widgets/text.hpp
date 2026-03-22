#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Displays a string of text.
     *
     * Wraps `RawText` with a `TextSpan`. For styled text, construct the span
     * directly; for simple text use the convenience constructor.
     */
    class Text : public StatelessWidget
    {
    public:
        TextSpan span;

        /** @brief Convenience constructor for plain text with an optional style. */
        explicit Text(std::string text, TextStyle style = {})
            : span{std::move(text), std::move(style)}
        {}

        WidgetRef build(BuildContext& context) const override;
    };

} // namespace systems::leal::campello_widgets
