#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Linux implementation of PlatformMenuDelegate.
     *
     * X11/Wayland have no native menu-bar concept (unlike NSMenu on macOS
     * or HMENU on Windows), and there is no cross-desktop-environment
     * native replacement short of adopting a whole toolkit (GTK/Qt) this
     * project otherwise doesn't depend on. So this delegate does not talk
     * to any native API — it just reports that fact via
     * needsInWindowMenuBar(), and PlatformMenuBarView (a normal widget,
     * placed explicitly in the widget tree by the app) renders the menu
     * bar itself from the same PlatformMenu data, including keyboard
     * accelerators. See platform_menu_bar_view.hpp/.cpp.
     */
    class LinuxPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        void setMenus(const std::vector<PlatformMenuRef>& /*menus*/) override
        {
            // No native menu bar to update — PlatformMenuBarView reads the
            // menu structure directly from PlatformMenuBar::menusOf().
        }

        void clearMenus() override
        {
        }

        bool needsInWindowMenuBar() const override { return true; }
    };

    void initializeLinuxPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<LinuxPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets

// C interface for run_app.cpp to call
extern "C" void campello_widgets_initialize_linux_menu_delegate()
{
    systems::leal::campello_widgets::initializeLinuxPlatformMenuDelegate();
}
