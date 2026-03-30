#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief iOS implementation of PlatformMenuDelegate.
     *
     * iOS doesn't have a concept of a menu bar like desktop platforms.
     * This is a no-op implementation.
     */
    class IOSPlatformMenuDelegate : public NoOpPlatformMenuDelegate
    {
    };

    void initializeIOSPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<IOSPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets

// C interface for run_app.mm to call
extern "C" void campello_widgets_initialize_ios_menu_delegate()
{
    systems::leal::campello_widgets::initializeIOSPlatformMenuDelegate();
}
