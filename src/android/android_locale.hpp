#pragma once

#include <string>

namespace systems::leal::campello_widgets::detail
{
    // The activity's configuration locale ("es-ES"), set by run_app.cpp
    // whenever it reads the configuration; Locale::systemLocales() reads it.
    void setAndroidSystemLocale(std::string tag);
    // The activity's internal data folder, for SharedPreferences.
    void setAndroidDataPath(std::string path);
}
