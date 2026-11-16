#include <campello_widgets/ui/shared_preferences.hpp>

#include <emscripten.h>

#include <cstdlib>

namespace systems::leal::campello_widgets::detail
{
    namespace
    {
        EM_JS(char*, cw_load_shared_preferences, (), {
            let text = '';
            try { text = window.localStorage.getItem('campello_widgets.shared_preferences') || ''; } catch (e) {}
            return stringToNewUTF8(text);
        });
        EM_JS(void, cw_save_shared_preferences, (const char* text), {
            try { window.localStorage.setItem('campello_widgets.shared_preferences', UTF8ToString(text)); } catch (e) {}
        });
    }

    // The browser's localStorage (one item holding them all).
    SharedPreferences::Store platformPreferencesStore()
    {
        SharedPreferences::Store store;
        store.load = [] {
            char* text = cw_load_shared_preferences();
            std::string out = text != nullptr ? text : "";
            std::free(text);
            return out;
        };
        store.save = [](const std::string& text) { cw_save_shared_preferences(text.c_str()); };
        return store;
    }
}
