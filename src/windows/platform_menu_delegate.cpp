#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Windows implementation of PlatformMenuDelegate.
     *
     * This is a stub implementation that does nothing. A full implementation
     * would use Win32 menu APIs (HMENU, CreateMenu, etc.) to create a native
     * menu bar on Windows.
     */
    class WindowsPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        void setMenus(const std::vector<PlatformMenuRef>& /*menus*/) override
        {
            // TODO: Implement using Win32 HMENU API
            // - Create menu bar
            // - Add menus and items
            // - Register keyboard accelerators
            // - Handle WM_COMMAND messages
        }

        void clearMenus() override
        {
            // TODO: Remove custom menus and restore default
        }
    };

    void initializeWindowsPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<WindowsPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets

// C interface for run_app.cpp to call
extern "C" void campello_widgets_initialize_windows_menu_delegate()
{
    systems::leal::campello_widgets::initializeWindowsPlatformMenuDelegate();
}
