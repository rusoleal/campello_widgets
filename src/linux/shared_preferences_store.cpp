#include "../ui/shared_preferences_file_store.hpp"

#include <cstdlib>

namespace systems::leal::campello_widgets::detail
{
    // $XDG_DATA_HOME (or ~/.local/share)/<executable name>/shared_preferences.txt
    SharedPreferences::Store platformPreferencesStore()
    {
        std::error_code ignored;
        const auto name = std::filesystem::read_symlink("/proc/self/exe", ignored).filename();
        std::filesystem::path base;
        if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0') {
            base = data;
        } else if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
            base = std::filesystem::path(home) / ".local" / "share";
        }
        if (base.empty() || name.empty()) return fileStore({});
        return fileStore(base / name / "shared_preferences.txt");
    }
}
