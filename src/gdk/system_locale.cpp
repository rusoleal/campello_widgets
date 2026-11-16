#include <campello_widgets/ui/locale.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace systems::leal::campello_widgets
{

    // GDK (Gaming.Desktop): the user's default locale name.
    std::vector<Locale> Locale::systemLocales()
    {
        std::vector<Locale> out;
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
        if (const int length = GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH); length > 1)
        {
            const int size = WideCharToMultiByte(CP_UTF8, 0, name, length - 1, nullptr, 0, nullptr, nullptr);
            std::string tag(static_cast<std::size_t>(size > 0 ? size : 0), '\0');
            if (size > 0) WideCharToMultiByte(CP_UTF8, 0, name, length - 1, tag.data(), size, nullptr, nullptr);
            Locale locale = Locale::parse(tag);
            if (!locale.language.empty()) out.push_back(locale);
        }
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
