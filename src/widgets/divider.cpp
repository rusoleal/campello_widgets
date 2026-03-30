#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Divider::build(BuildContext&) const
    {
        // A coloured box of the desired thickness, indented on both sides.
        auto line     = std::make_shared<Container>();
        line->height  = thickness;
        line->color   = color;

        auto indented         = std::make_shared<Padding>();
        indented->padding     = EdgeInsets::only(indent, 0.0f, end_indent, 0.0f);
        indented->child       = line;

        auto box      = std::make_shared<SizedBox>();
        box->height   = height;
        box->child    = std::make_shared<Align>(Alignment::center(), indented);
        return box;
    }

} // namespace systems::leal::campello_widgets
