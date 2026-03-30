#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Android implementation of PlatformMenuDelegate.
     *
     * Android doesn't have a concept of a menu bar like desktop platforms.
     * This is a no-op implementation.
     */
    class AndroidPlatformMenuDelegate : public NoOpPlatformMenuDelegate
    {
    };

    void initializeAndroidPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<AndroidPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets

// C interface for run_app.cpp to call
extern "C" void campello_widgets_initialize_android_menu_delegate()
{
    systems::leal::campello_widgets::initializeAndroidPlatformMenuDelegate();
}
