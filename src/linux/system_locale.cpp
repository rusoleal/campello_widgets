#include <campello_widgets/ui/locale.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace systems::leal::campello_widgets
{

    // Linux (glibc's order): LANGUAGE, a colon-separated preference list
    // (honoured unless the locale is "C"), then LC_ALL, LC_MESSAGES, LANG.
    std::vector<Locale> Locale::systemLocales()
    {
        std::vector<Locale> out;
        const auto env = [](const char* name) {
            const char* v = std::getenv(name);
            return std::string(v ? v : "");
        };
        const auto add = [&out](const std::string& tag) {
            Locale locale = Locale::parse(tag);
            if (!locale.language.empty() && std::find(out.begin(), out.end(), locale) == out.end()) out.push_back(locale);
        };
        std::string messages = env("LC_ALL");
        if (messages.empty()) messages = env("LC_MESSAGES");
        if (messages.empty()) messages = env("LANG");
        if (messages != "C" && messages != "POSIX")
        {
            const std::string list = env("LANGUAGE");
            std::size_t start = 0;
            while (start <= list.size())
            {
                const std::size_t end = list.find(':', start);
                const std::string item = list.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!item.empty()) add(item);
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        if (!messages.empty()) add(messages);
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
