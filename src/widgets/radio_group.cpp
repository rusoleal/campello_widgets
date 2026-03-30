#include <campello_widgets/widgets/radio_group.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef RadioGroup::build(BuildContext&) const
    {
        auto scope             = std::make_shared<RadioGroupScope>();
        scope->group_value     = group_value;
        scope->on_changed      = on_changed;
        scope->child           = child;
        return scope;
    }

} // namespace systems::leal::campello_widgets
