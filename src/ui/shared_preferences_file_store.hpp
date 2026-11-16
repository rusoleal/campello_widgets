#pragma once
// A SharedPreferences store in a file (Windows, Linux, Android, GDK).

#include <campello_widgets/ui/shared_preferences.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace systems::leal::campello_widgets::detail
{
    inline SharedPreferences::Store fileStore(std::filesystem::path file)
    {
        SharedPreferences::Store store;
        store.load = [file] {
            if (file.empty()) return std::string();
            std::ifstream in(file, std::ios::binary);
            std::stringstream s;
            s << in.rdbuf();
            return s.str();
        };
        store.save = [file](const std::string& text) {
            if (file.empty()) return;
            std::error_code ignored;
            std::filesystem::create_directories(file.parent_path(), ignored);
            // Written beside, then renamed over: a crash never leaves half a file.
            auto temp = file;
            temp += ".tmp";
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                out << text;
            }
            std::filesystem::rename(temp, file, ignored);
        };
        return store;
    }
}
