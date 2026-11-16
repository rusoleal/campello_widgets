#include <campello_widgets/ui/shared_preferences.hpp>

#include <cstdio>
#include <cstdlib>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // Backslash escapes for the separators: tab, newline, backslash.
        std::string escape(std::string_view text)
        {
            std::string out;
            for (const char c : text)
            {
                if (c == '\\') out += "\\\\";
                else if (c == '\t') out += "\\t";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else out += c;
            }
            return out;
        }

        std::string unescape(std::string_view text)
        {
            std::string out;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] != '\\' || i + 1 == text.size()) { out += text[i]; continue; }
                const char c = text[++i];
                out += c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c;
            }
            return out;
        }
    }

    // One line per value: <type> TAB <key> TAB <value>; types b, i, d, s.
    std::string SharedPreferences::encode(const std::map<std::string, Value, std::less<>>& values)
    {
        std::string out;
        for (const auto& [key, value] : values)
        {
            std::string type, text;
            if (const auto* b = std::get_if<bool>(&value)) { type = "b"; text = *b ? "1" : "0"; }
            else if (const auto* i = std::get_if<int64_t>(&value)) { type = "i"; text = std::to_string(*i); }
            else if (const auto* d = std::get_if<double>(&value))
            {
                char buffer[40];
                std::snprintf(buffer, sizeof(buffer), "%.17g", *d);
                type = "d";
                text = buffer;
            }
            else { type = "s"; text = escape(std::get<std::string>(value)); }
            out += type + "\t" + escape(key) + "\t" + text + "\n";
        }
        return out;
    }

    std::map<std::string, SharedPreferences::Value, std::less<>> SharedPreferences::decode(std::string_view text)
    {
        std::map<std::string, Value, std::less<>> out;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            const std::string_view line = text.substr(start, end - start);
            start = end + 1;
            const auto t1 = line.find('\t');
            const auto t2 = t1 == std::string_view::npos ? t1 : line.find('\t', t1 + 1);
            if (t2 == std::string_view::npos) continue;  // not a value line
            const std::string_view type = line.substr(0, t1);
            const std::string key = unescape(line.substr(t1 + 1, t2 - t1 - 1));
            const std::string value(line.substr(t2 + 1));
            if (type == "b") out[key] = value == "1";
            else if (type == "i") out[key] = static_cast<int64_t>(std::strtoll(value.c_str(), nullptr, 10));
            else if (type == "d") out[key] = std::strtod(value.c_str(), nullptr);
            else if (type == "s") out[key] = unescape(value);
        }
        return out;
    }

    SharedPreferences& SharedPreferences::instance()
    {
        static SharedPreferences prefs(detail::platformPreferencesStore());
        return prefs;
    }

    SharedPreferences::SharedPreferences(Store store)
        : store_(std::move(store))
    {
        if (store_.load) values_ = decode(store_.load());
    }

    std::optional<bool> SharedPreferences::getBool(std::string_view key) const
    {
        const auto it = values_.find(key);
        if (it == values_.end()) return std::nullopt;
        if (const auto* v = std::get_if<bool>(&it->second)) return *v;
        return std::nullopt;
    }

    std::optional<int64_t> SharedPreferences::getInt(std::string_view key) const
    {
        const auto it = values_.find(key);
        if (it == values_.end()) return std::nullopt;
        if (const auto* v = std::get_if<int64_t>(&it->second)) return *v;
        return std::nullopt;
    }

    std::optional<double> SharedPreferences::getDouble(std::string_view key) const
    {
        const auto it = values_.find(key);
        if (it == values_.end()) return std::nullopt;
        if (const auto* v = std::get_if<double>(&it->second)) return *v;
        if (const auto* v = std::get_if<int64_t>(&it->second)) return static_cast<double>(*v);
        return std::nullopt;
    }

    std::optional<std::string> SharedPreferences::getString(std::string_view key) const
    {
        const auto it = values_.find(key);
        if (it == values_.end()) return std::nullopt;
        if (const auto* v = std::get_if<std::string>(&it->second)) return *v;
        return std::nullopt;
    }

    bool SharedPreferences::setBool(std::string_view key, bool value) { return set(key, value); }
    bool SharedPreferences::setInt(std::string_view key, int64_t value) { return set(key, value); }
    bool SharedPreferences::setDouble(std::string_view key, double value) { return set(key, value); }
    bool SharedPreferences::setString(std::string_view key, std::string_view value) { return set(key, std::string(value)); }

    bool SharedPreferences::containsKey(std::string_view key) const
    {
        return values_.find(key) != values_.end();
    }

    std::vector<std::string> SharedPreferences::keys() const
    {
        std::vector<std::string> out;
        for (const auto& [key, value] : values_) out.push_back(key);
        return out;
    }

    bool SharedPreferences::remove(std::string_view key)
    {
        const auto it = values_.find(key);
        if (it == values_.end()) return true;
        values_.erase(it);
        return persist();
    }

    bool SharedPreferences::clear()
    {
        values_.clear();
        return persist();
    }

    bool SharedPreferences::set(std::string_view key, Value value)
    {
        values_[std::string(key)] = std::move(value);
        return persist();
    }

    bool SharedPreferences::persist()
    {
        if (!store_.save) return false;
        store_.save(encode(values_));
        return true;
    }

} // namespace systems::leal::campello_widgets
