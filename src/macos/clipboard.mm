#include <campello_widgets/ui/clipboard.hpp>

#import <AppKit/AppKit.h>

namespace systems::leal::campello_widgets
{

    void Clipboard::setText(const std::string& text)
    {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSString* str = [NSString stringWithUTF8String:text.c_str()];
        if (str)
            [pb setString:str forType:NSPasteboardTypeString];
    }

    std::string Clipboard::getText()
    {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (!str)
            return {};
        return std::string([str UTF8String]);
    }

} // namespace systems::leal::campello_widgets
