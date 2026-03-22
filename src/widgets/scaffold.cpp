#include <campello_widgets/widgets/scaffold.hpp>
#include <campello_widgets/widgets/colored_box.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Scaffold::build(BuildContext&) const
    {
        auto box   = std::make_shared<ColoredBox>();
        box->color = background_color;
        box->child = child;
        return box;
    }

} // namespace systems::leal::campello_widgets
