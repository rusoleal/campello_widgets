#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/raw_text.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Text::build(BuildContext&) const
    {
        return RawText::create(span.text, span.style);
    }

} // namespace systems::leal::campello_widgets
