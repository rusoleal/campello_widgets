#include <campello_widgets/ui/clipboard.hpp>

// TODO: not yet implemented -- port src/macos/clipboard.mm's approach to
// UIPasteboard (UIPasteboard.general, .string / .setValue:forPasteboardType:)
// when iOS clipboard support is needed. Stubbed for now so the library
// still links on this platform.

namespace systems::leal::campello_widgets
{

    void Clipboard::setText(const std::string&) {}

    std::string Clipboard::getText() { return {}; }

} // namespace systems::leal::campello_widgets
