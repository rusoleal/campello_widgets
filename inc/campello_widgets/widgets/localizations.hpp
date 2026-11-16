#pragma once

#include <campello_widgets/ui/locale.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Provides the app's locale to the widgets below it (Flutter's
     * `Localizations`); a change rebuilds the widgets that read it.
     *
     * Typical use: resolve the system's preferred locales against the
     * languages the app is translated into, and put the result at the root:
     *
     * @code
     * const Locale en{"en"}, es{"es"};
     * auto locale = resolveLocale(Locale::systemLocales(), {en, es});
     * auto root   = std::make_shared<Localizations>(locale, app);
     * // in a build(): Localizations::localeOf(ctx).language == "es" ? ... : ...
     * @endcode
     */
    class Localizations : public InheritedWidget
    {
    public:
        Locale locale;

        Localizations(Locale l, WidgetRef c)
            : locale(std::move(l))
        {
            child = std::move(c);
        }

        /** @brief The nearest Localizations' locale, or nullptr (registers a dependency). */
        static const Locale* maybeLocaleOf(BuildContext& context);

        /** @brief The nearest Localizations' locale; English when there's none. */
        static Locale localeOf(BuildContext& context);

        bool updateShouldNotify(const InheritedWidget& old_widget) const override;
    };

} // namespace systems::leal::campello_widgets
