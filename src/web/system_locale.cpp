#include <campello_widgets/ui/locale.hpp>

#include <emscripten.h>

#include <string>

namespace systems::leal::campello_widgets
{

    // The web: navigator.languages (the browser's preference list), else
    // navigator.language.
    std::vector<Locale> Locale::systemLocales()
    {
        std::vector<Locale> out;
        const char* list = emscripten_run_script_string(
            "(navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language || 'en']).join(',')");
        const std::string tags = list ? list : "";
        std::size_t start = 0;
        while (start <= tags.size())
        {
            const std::size_t end = tags.find(',', start);
            Locale locale = Locale::parse(tags.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (!locale.language.empty()) out.push_back(locale);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
