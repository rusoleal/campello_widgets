#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Small settings remembered between runs (Flutter's
     * `shared_preferences`): typed values by key, kept by the platform.
     *
     * Where they're kept:
     * - macOS, iOS: `NSUserDefaults` (the app's standard defaults)
     * - Windows: `%APPDATA%\<executable name>\shared_preferences.txt`
     * - Linux: `$XDG_DATA_HOME` (or `~/.local/share`)`/<executable name>/shared_preferences.txt`
     * - Android: the activity's internal data folder
     * - GDK: `%LOCALAPPDATA%\<executable name>\shared_preferences.txt`
     * - Web: the browser's `localStorage`
     *
     * Synchronous: values are read once (the first `instance()`) and every
     * change is written at once. For settings, not documents or large data.
     * UI thread.
     *
     * @code
     * auto& prefs = SharedPreferences::instance();
     * const bool dark = prefs.getBool("dark_mode").value_or(false);
     * prefs.setBool("dark_mode", !dark);
     * @endcode
     */
    class SharedPreferences
    {
    public:
        using Value = std::variant<bool, int64_t, double, std::string>;

        /** @brief Where the values are loaded from and saved to, as one text. */
        struct Store
        {
            std::function<std::string()>                 load;
            std::function<void(const std::string& text)> save;
        };

        /** @brief The app's preferences, in the platform's store. */
        static SharedPreferences& instance();

        /** @brief Preferences in `store` (tests, or an app's own location). */
        explicit SharedPreferences(Store store);

        std::optional<bool>        getBool(std::string_view key) const;
        std::optional<int64_t>     getInt(std::string_view key) const;
        std::optional<double>      getDouble(std::string_view key) const;  ///< ints read as doubles too
        std::optional<std::string> getString(std::string_view key) const;

        /** @brief Saves a value (replacing one of any type). False if it couldn't be saved. */
        bool setBool(std::string_view key, bool value);
        bool setInt(std::string_view key, int64_t value);
        bool setDouble(std::string_view key, double value);
        bool setString(std::string_view key, std::string_view value);

        bool containsKey(std::string_view key) const;
        std::vector<std::string> keys() const;
        bool remove(std::string_view key);
        bool clear();

        /** @brief The values as the store keeps them, and back (one per line; tested). */
        static std::string encode(const std::map<std::string, Value, std::less<>>& values);
        static std::map<std::string, Value, std::less<>> decode(std::string_view text);

    private:
        bool set(std::string_view key, Value value);
        bool persist();

        Store store_;
        std::map<std::string, Value, std::less<>> values_;
    };

    namespace detail
    {
        /** @brief The platform's store (one implementation per platform). */
        SharedPreferences::Store platformPreferencesStore();
    }

} // namespace systems::leal::campello_widgets
