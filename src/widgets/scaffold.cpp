#include <campello_widgets/widgets/scaffold.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Scaffold::build(BuildContext& context) const
    {
        auto box   = std::make_shared<ColoredBox>();
        box->color = background_color.value_or(Theme::tokensOf(context)->colors.background);
        box->child = child;
        return box;
    }

} // namespace systems::leal::campello_widgets
