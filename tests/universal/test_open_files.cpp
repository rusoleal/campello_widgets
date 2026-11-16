// Files the platform asks the app to open: queued until a handler is set.

#include <gtest/gtest.h>

#include <campello_widgets/ui/open_files.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(OpenFiles, QueuedUntilAHandlerIsSet)
{
    cw::setOpenFilesHandler(nullptr);
    cw::deliverOpenFiles({"/a.cbim"});
    cw::deliverOpenFiles({"/b.cbim"});
    std::vector<std::string> got;
    cw::setOpenFilesHandler([&](const std::vector<std::string>& paths) { got.insert(got.end(), paths.begin(), paths.end()); });
    EXPECT_EQ(got, (std::vector<std::string>{"/a.cbim", "/b.cbim"}));
    // Then straight to it; nothing queued twice.
    cw::deliverOpenFiles({"/c.cbim"});
    EXPECT_EQ(got, (std::vector<std::string>{"/a.cbim", "/b.cbim", "/c.cbim"}));
    cw::setOpenFilesHandler(nullptr);
    std::vector<std::string> again;
    cw::setOpenFilesHandler([&](const std::vector<std::string>& paths) { again = paths; });
    EXPECT_TRUE(again.empty());
    cw::setOpenFilesHandler(nullptr);
}
