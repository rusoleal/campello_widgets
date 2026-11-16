#include <campello_widgets/ui/locale.hpp>

#import <Foundation/Foundation.h>

namespace systems::leal::campello_widgets
{

    // macOS: the user's preferred languages (System Settings > Language & Region).
    std::vector<Locale> Locale::systemLocales()
    {
        std::vector<Locale> out;
        @autoreleasepool
        {
            for (NSString* tag in NSLocale.preferredLanguages)
            {
                Locale locale = Locale::parse(tag.UTF8String);
                if (!locale.language.empty()) out.push_back(locale);
            }
        }
        if (out.empty()) out.push_back(Locale{"en", "", ""});
        return out;
    }

} // namespace systems::leal::campello_widgets
