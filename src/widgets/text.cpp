#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/raw_text.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Text::build(BuildContext&) const
    {
        return RawText::create(span.text, span.style);
    }


    void Text::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<StringProperty>("text", span.text));
    }
} // namespace systems::leal::campello_widgets
