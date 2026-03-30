#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Linux implementation of PlatformMenuDelegate.
     *
     * This is a stub implementation that does nothing. A full implementation
     * would use GTK or Qt menu APIs to create a native menu bar on Linux.
     * The implementation would depend on which toolkit the application uses.
     */
    class LinuxPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        void setMenus(const std::vector<PlatformMenuRef>& /*menus*/) override
        {
            // TODO: Implement using GTK or Qt menu API
            // - Create menu bar (GtkMenuBar or QMenuBar)
            // - Add menus and items
            // - Register keyboard accelerators
        }

        void clearMenus() override
        {
            // TODO: Remove custom menus and restore default
        }
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
