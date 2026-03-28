#include <campello_widgets/ui/inline_span.hpp>
#include <campello_widgets/ui/draw_backend.hpp>

namespace systems::leal::campello_widgets
{

    float InlineTextSpan::computeIntrinsicWidth(IDrawBackend* backend) const
    {
        if (!backend || text_content.empty()) {
            // Fallback: estimate based on character count
            const float char_width = text_style.font_size * 0.6f;
            return char_width * static_cast<float>(text_content.size());
        }

        return backend->measureText(TextSpan{text_content, text_style}).width;
    }

} // namespace systems::leal::campello_widgets
