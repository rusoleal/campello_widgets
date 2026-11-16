#include <campello_widgets/ui/open_files.hpp>

namespace systems::leal::campello_widgets
{

    namespace
    {
        OpenFilesHandler& handler()
        {
            static OpenFilesHandler h;
            return h;
        }
        std::vector<std::string>& pending()
        {
            static std::vector<std::string> p;
            return p;
        }
    }

    void setOpenFilesHandler(OpenFilesHandler h)
    {
        handler() = std::move(h);
        if (handler() && !pending().empty()) {
            const auto paths = std::move(pending());
            pending().clear();
            handler()(paths);
        }
    }

    void deliverOpenFiles(const std::vector<std::string>& paths)
    {
        if (paths.empty()) return;
        if (handler()) {
            handler()(paths);
        } else {
            pending().insert(pending().end(), paths.begin(), paths.end());
        }
    }

} // namespace systems::leal::campello_widgets
