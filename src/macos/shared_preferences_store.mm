#include <campello_widgets/ui/shared_preferences.hpp>

#import <Foundation/Foundation.h>

namespace systems::leal::campello_widgets::detail
{
    // NSUserDefaults (the app's standard defaults, as Flutter's
    // shared_preferences): one string value holding them all.
    SharedPreferences::Store platformPreferencesStore()
    {
        static NSString* const KEY = @"campello_widgets.shared_preferences";
        SharedPreferences::Store store;
        store.load = [] {
            @autoreleasepool
            {
                NSString* text = [NSUserDefaults.standardUserDefaults stringForKey:KEY];
                return text != nil ? std::string(text.UTF8String) : std::string();
            }
        };
        store.save = [](const std::string& text) {
            @autoreleasepool
            {
                NSString* value = [NSString stringWithUTF8String:text.c_str()];
                [NSUserDefaults.standardUserDefaults setObject:value != nil ? value : @"" forKey:KEY];
            }
        };
        return store;
    }
}
