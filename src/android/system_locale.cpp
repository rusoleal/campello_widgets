#include <campello_widgets/ui/locale.hpp>

#include "android_locale.hpp"

#include <mutex>

namespace systems::leal::campello_widgets
{

    namespace
    {
        std::mutex g_mutex;
        std::string g_tag;
    }

    void detail::setAndroidSystemLocale(std::string tag)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_tag = std::move(tag);
    }

    // Android: the activity configuration's language and country (the
    // device's first language; Android's full preference list needs JNI).
    std::vector<Locale> Locale::systemLocales()
    {
        std::string tag;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            tag = g_tag;
        }
        std::vector<Locale> out;
        Locale locale = Locale::parse(tag);
        if (!locale.language.empty()) out.push_back(locale);
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
