#include "../ui/shared_preferences_file_store.hpp"

#include "android_locale.hpp"

#include <mutex>

namespace systems::leal::campello_widgets::detail
{
    namespace
    {
        std::mutex g_mutex;
        std::string g_dataPath;
    }

    void setAndroidDataPath(std::string path)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dataPath = std::move(path);
    }

    // The activity's internal data folder (set by run_app.cpp).
    SharedPreferences::Store platformPreferencesStore()
    {
        std::string path;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            path = g_dataPath;
        }
        if (path.empty()) return fileStore({});
        return fileStore(std::filesystem::path(path) / "shared_preferences.txt");
    }
}
