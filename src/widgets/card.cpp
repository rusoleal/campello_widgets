#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Card::build(BuildContext&) const
    {
        BoxDecoration deco;
        deco.color         = color;
        deco.border_radius = border_radius;
        if (elevation > 0.0f) {
            deco.box_shadow = {BoxShadow{
                Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.2f),
                Offset{0.0f, elevation},
                elevation * 2.0f
            }};
        }

        auto decorated        = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = child;

        if (margin == EdgeInsets::zero()) return decorated;

        auto padded     = std::make_shared<Padding>();
        padded->padding = margin;
        padded->child   = decorated;
        return padded;
    }

} // namespace systems::leal::campello_widgets
