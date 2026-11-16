#include "../ui/shared_preferences_file_store.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdlib>

namespace systems::leal::campello_widgets::detail
{
    // %LOCALAPPDATA%\<executable name>\shared_preferences.txt
    SharedPreferences::Store platformPreferencesStore()
    {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const auto name = std::filesystem::path(exe).stem();
        const wchar_t* appData = _wgetenv(L"LOCALAPPDATA");
        if (appData == nullptr || name.empty()) return fileStore({});
        return fileStore(std::filesystem::path(appData) / name / "shared_preferences.txt");
    }
}
