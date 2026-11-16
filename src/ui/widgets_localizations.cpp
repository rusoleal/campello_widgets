#include <campello_widgets/ui/widgets_localizations.hpp>
#include <campello_widgets/widgets/localizations.hpp>

namespace systems::leal::campello_widgets
{

    const WidgetsLocalizations& WidgetsLocalizations::forLocale(const Locale& locale)
    {
        static const WidgetsLocalizations en{"More", "Cancel", "Close", "Search", "Cut", "Copy", "Paste", "Select All"};
        static const WidgetsLocalizations es{"Más", "Cancelar", "Cerrar", "Buscar", "Cortar", "Copiar", "Pegar", "Seleccionar todo"};
        return locale.language == "es" ? es : en;
    }

    const WidgetsLocalizations& WidgetsLocalizations::of(BuildContext& context)
    {
        return forLocale(Localizations::localeOf(context));
    }

} // namespace systems::leal::campello_widgets
