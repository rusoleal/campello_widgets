#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A language, optionally with its script and region (Flutter's
     * `Locale`): "en", "es-ES", "zh-Hans-CN".
     *
     * Parsed from BCP 47 tags ("es-ES") and POSIX locale names
     * ("es_ES.UTF-8@euro"); "C" and "POSIX" mean English. Stored normalized:
     * language lowercase, script titlecase, region uppercase.
     */
    struct Locale
    {
        std::string language;  ///< ISO 639 ("es"); empty: unknown
        std::string script;    ///< ISO 15924 ("Hans"); usually empty
        std::string region;    ///< ISO 3166 or UN M.49 ("ES", "419"); may be empty

        /** @brief Parses a BCP 47 tag or POSIX locale name (see the class doc). */
        static Locale parse(std::string_view tag);

        /** @brief "es-ES", "zh-Hans-CN", "en". */
        std::string toLanguageTag() const;

        /**
         * @brief The user's preferred locales, most preferred first, as the
         * platform reports them: macOS/iOS `NSLocale.preferredLanguages`,
         * Windows the user's preferred UI languages, Linux `LANGUAGE` (a
         * list), then `LC_ALL`, `LC_MESSAGES`, `LANG`, the web
         * `navigator.languages`, Android the activity's configuration. Read
         * on each call. Never empty: English when the platform says nothing.
         */
        static std::vector<Locale> systemLocales();

        bool operator==(const Locale&) const = default;
    };

    /**
     * @brief The supported locale that best matches the preferred ones
     * (Flutter's basic locale list resolution): for each preferred locale
     * in order, an exact match, then one with the same language and script,
     * then the same language and region, then the same language; failing
     * every preferred locale, the first supported one. `supported` must not
     * be empty (an empty Locale is returned if it is).
     */
    Locale resolveLocale(const std::vector<Locale>& preferred, const std::vector<Locale>& supported);

} // namespace systems::leal::campello_widgets
