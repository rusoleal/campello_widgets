#include <campello_widgets/widgets/default_text_style.hpp>

namespace systems::leal::campello_widgets
{

    TextStyle DefaultTextStyle::of(BuildContext& context)
    {
        const auto* widget = context.dependOnInheritedWidgetOfExactType<DefaultTextStyle>();
        if (widget) return widget->style;
        return TextStyle{};
    }

    bool DefaultTextStyle::updateShouldNotify(const InheritedWidget& old_widget) const
    {
        const auto& old_style = static_cast<const DefaultTextStyle&>(old_widget);
        return style != old_style.style;
    }

} // namespace systems::leal::campello_widgets
