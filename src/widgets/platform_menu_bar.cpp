#include <campello_widgets/widgets/platform_menu_bar.hpp>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef PlatformMenuBar::build(BuildContext& /*context*/) const
    {
        // Update the platform menu bar with our menus
        PlatformMenuDelegate::instance()->setMenus(menus);

        // Wrap the child in an InheritedWidget so PlatformMenuBarView (or
        // any other descendant) can recover the menu structure via
        // menusOf(), regardless of whether the platform renders it
        // natively or needs it drawn in the widget tree.
        return std::make_shared<detail::PlatformMenuScope>(menus, child);
    }

    const std::vector<PlatformMenuRef>* PlatformMenuBar::menusOf(BuildContext& context)
    {
        const auto* scope = context.dependOnInheritedWidgetOfExactType<detail::PlatformMenuScope>();
        return scope ? &scope->menus : nullptr;
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
