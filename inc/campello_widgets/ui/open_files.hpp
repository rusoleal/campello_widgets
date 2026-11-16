#pragma once

#include <functional>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Files the platform asks the app to open: on macOS a document
     * double-clicked in Finder, dropped on the Dock icon or opened with
     * `open -a App file` (the app's Info.plist declares the document
     * types it handles: CFBundleDocumentTypes).
     *
     * These don't arrive as command-line arguments. At launch they can
     * come before the app has set its handler, so they're queued and
     * delivered when it is set. The handler runs on the UI thread.
     *
     * @code
     * setOpenFilesHandler([doc](const std::vector<std::string>& paths) {
     *     doc->open(paths.front());
     * });
     * @endcode
     */
    using OpenFilesHandler = std::function<void(const std::vector<std::string>& paths)>;

    /// Sets (or, with nullptr, clears) the handler; files queued so far
    /// go to it at once.
    void setOpenFilesHandler(OpenFilesHandler handler);

    /// Platform side (and tests): files to open, to the handler or queued
    /// until one is set. UI thread.
    void deliverOpenFiles(const std::vector<std::string>& paths);

} // namespace systems::leal::campello_widgets
