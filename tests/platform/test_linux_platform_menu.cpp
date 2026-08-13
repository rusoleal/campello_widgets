#if defined(__linux__)

#include <gtest/gtest.h>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace cw = systems::leal::campello_widgets;

extern "C" void campello_widgets_initialize_linux_menu_delegate();

TEST(LinuxPlatformMenuDelegate, NeedsInWindowMenuBarIsTrue)
{
    campello_widgets_initialize_linux_menu_delegate();

    EXPECT_TRUE(cw::PlatformMenuDelegate::instance()->needsInWindowMenuBar());

    // Don't leak this override into other tests sharing the process.
    cw::PlatformMenuDelegate::setInstance(nullptr);
}

#endif // defined(__linux__)
