#include <campello_widgets/ui/locale.hpp>

#include <algorithm>
#include <cctype>

namespace systems::leal::campello_widgets
{

    Locale Locale::parse(std::string_view tag)
    {
        // POSIX: drop ".charset" and "@modifier"; "_" separates like "-".
        if (const auto cut = tag.find_first_of(".@"); cut != std::string_view::npos) tag = tag.substr(0, cut);
        Locale out;
        if (tag.empty() || tag == "C" || tag == "POSIX")
        {
            out.language = tag.empty() ? "" : "en";
            return out;
        }
        std::vector<std::string> parts;
        std::string part;
        for (const char c : tag)
        {
            if (c == '-' || c == '_')
            {
                parts.push_back(part);
                part.clear();
            }
            else
            {
                part += c;
            }
        }
        parts.push_back(part);
        const auto lower = [](std::string s) {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        const auto upper = [](std::string s) {
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        };
        out.language = lower(parts[0]);
        for (std::size_t i = 1; i < parts.size(); ++i)
        {
            const std::string& p = parts[i];
            const bool letters = !p.empty() && std::all_of(p.begin(), p.end(), [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; });
            const bool digits = !p.empty() && std::all_of(p.begin(), p.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
            if (p.size() == 4 && letters && out.script.empty() && out.region.empty())
            {
                out.script = lower(p);
                out.script[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out.script[0])));
            }
            else if (((p.size() == 2 && letters) || (p.size() == 3 && digits)) && out.region.empty())
            {
                out.region = upper(p);
            }
            // Variants and extensions ("-u-ca-...") are ignored.
        }
        return out;
    }

    std::string Locale::toLanguageTag() const
    {
        std::string tag = language;
        if (!script.empty()) tag += "-" + script;
        if (!region.empty()) tag += "-" + region;
        return tag;
    }

    Locale resolveLocale(const std::vector<Locale>& preferred, const std::vector<Locale>& supported)
    {
        if (supported.empty()) return {};
        for (const auto& want : preferred)
        {
            if (want.language.empty()) continue;
            for (const auto& s : supported)
                if (s == want) return s;
            if (!want.script.empty())
                for (const auto& s : supported)
                    if (s.language == want.language && s.script == want.script) return s;
            if (!want.region.empty())
                for (const auto& s : supported)
                    if (s.language == want.language && s.region == want.region) return s;
            for (const auto& s : supported)
                if (s.language == want.language) return s;
        }
        return supported.front();
    }

} // namespace systems::leal::campello_widgets
