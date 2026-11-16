#include <campello_widgets/widgets/localizations.hpp>

namespace systems::leal::campello_widgets
{

    const Locale* Localizations::maybeLocaleOf(BuildContext& context)
    {
        const auto* widget = context.dependOnInheritedWidgetOfExactType<Localizations>();
        return widget ? &widget->locale : nullptr;
    }

    Locale Localizations::localeOf(BuildContext& context)
    {
        if (const auto* locale = maybeLocaleOf(context)) return *locale;
        return Locale{"en", "", ""};
    }

    bool Localizations::updateShouldNotify(const InheritedWidget& old_widget) const
    {
        return static_cast<const Localizations&>(old_widget).locale != locale;
    }

} // namespace systems::leal::campello_widgets
