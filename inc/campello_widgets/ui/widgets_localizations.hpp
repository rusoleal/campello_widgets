#pragma once

#include <campello_widgets/ui/locale.hpp>
#include <campello_widgets/widgets/build_context.hpp>

#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief The library's own texts (Flutter's `WidgetsLocalizations` /
     * `MaterialLocalizations`): what widgets show without the app naming it,
     * like the toolbar's "More" tooltip. In the locale of the nearest
     * `Localizations` (English without one); English and Spanish so far,
     * English for any other language.
     */
    struct WidgetsLocalizations
    {
        std::string more;        ///< a toolbar's overflow button
        std::string cancel;
        std::string close;
        std::string search;
        std::string cut;
        std::string copy;
        std::string paste;
        std::string select_all;

        /** @brief The texts for `locale` (English when not translated). */
        static const WidgetsLocalizations& forLocale(const Locale& locale);

        /** @brief The texts for the nearest Localizations' locale (registers a dependency). */
        static const WidgetsLocalizations& of(BuildContext& context);
    };

} // namespace systems::leal::campello_widgets
