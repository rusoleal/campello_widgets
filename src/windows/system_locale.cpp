#include <campello_widgets/ui/locale.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace systems::leal::campello_widgets
{

    namespace
    {
        std::string narrow(const wchar_t* text, int length)
        {
            const int size = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
            std::string out(static_cast<std::size_t>(size > 0 ? size : 0), '\0');
            if (size > 0) WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), size, nullptr, nullptr);
            return out;
        }
    }

    // Windows: the user's preferred UI languages (Settings > Time & language),
    // else the user's default locale.
    std::vector<Locale> Locale::systemLocales()
    {
        std::vector<Locale> out;
        ULONG count = 0;
        ULONG size = 0;
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size) && size > 0)
        {
            std::wstring buffer(size, L'\0');
            if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buffer.data(), &size))
            {
                // A double-null-terminated list of names ("es-ES\0en-US\0\0").
                for (const wchar_t* p = buffer.c_str(); *p != L'\0'; p += wcslen(p) + 1)
                {
                    Locale locale = Locale::parse(narrow(p, static_cast<int>(wcslen(p))));
                    if (!locale.language.empty()) out.push_back(locale);
                }
            }
        }
        if (out.empty())
        {
            wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
            if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0)
            {
                Locale locale = Locale::parse(narrow(name, static_cast<int>(wcslen(name))));
                if (!locale.language.empty()) out.push_back(locale);
            }
        }
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
