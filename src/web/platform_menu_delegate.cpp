#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Web implementation of PlatformMenuDelegate.
     *
     * A page has no native menu bar: like Linux, this delegate only reports
     * needsInWindowMenuBar(), and PlatformMenuBarView (placed in the widget
     * tree by the app) draws the menus from PlatformMenuBar::menusOf().
     */
    class WebPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        void setMenus(const std::vector<PlatformMenuRef>& /*menus*/) override {}
        void clearMenus() override {}
        bool needsInWindowMenuBar() const override { return true; }
    };

    void initializeWebPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<WebPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets
