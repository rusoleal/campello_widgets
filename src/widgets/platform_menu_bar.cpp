#include <campello_widgets/widgets/platform_menu_bar.hpp>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef PlatformMenuBar::build(BuildContext& /*context*/) const
    {
        // Update the platform menu bar with our menus
        PlatformMenuDelegate::instance()->setMenus(menus);

        // Return the child - this widget has no visual representation
        return child;
    }

    std::shared_ptr<PlatformMenuBar> PlatformMenuBar::create(WidgetRef child)
    {
        auto bar = std::make_shared<PlatformMenuBar>();
        bar->child = std::move(child);
        return bar;
    }

    std::shared_ptr<PlatformMenuBar> PlatformMenuBar::create(std::vector<PlatformMenuRef> menus, WidgetRef child)
    {
        auto bar = std::make_shared<PlatformMenuBar>();
        bar->menus = std::move(menus);
        bar->child = std::move(child);
        return bar;
    }

} // namespace systems::leal::campello_widgets
