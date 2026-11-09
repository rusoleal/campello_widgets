// Clipboard on the web. The browser's clipboard is asynchronous (reads need
// a user gesture), while Clipboard::getText() is synchronous, so a local copy
// backs it: setText() updates it and writes through to navigator.clipboard,
// and run_app.cpp holds Ctrl/Cmd+V back until the page's paste event has
// stored the pasted text here (webClipboardSetLocal()).

#include <campello_widgets/ui/clipboard.hpp>

#include <emscripten/emscripten.h>

#include <string>

namespace systems::leal::campello_widgets
{

namespace {
std::string& localClipboard()
{
    static std::string text;
    return text;
}
} // namespace

void Clipboard::setText(const std::string& text)
{
    localClipboard() = text;
    MAIN_THREAD_EM_ASM({
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(UTF8ToString($0)).catch(function () {});
        }
    }, text.c_str());
}

std::string Clipboard::getText()
{
    return localClipboard();
}

void webClipboardSetLocal(const std::string& text)
{
    localClipboard() = text;
}

} // namespace systems::leal::campello_widgets

