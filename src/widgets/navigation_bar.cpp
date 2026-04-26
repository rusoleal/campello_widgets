#include <campello_widgets/widgets/navigation_bar.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef NavigationBar::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        NavigationBarConfig cfg;
        cfg.selected_index = selected_index;
        cfg.on_tap         = on_tap;

        for (const auto& item : items) {
            NavigationBarConfig::Item ci;
            ci.icon  = item.icon;
            ci.label = item.label;
            cfg.items.push_back(std::move(ci));
        }

        return ds->buildNavigationBar(cfg);
    }

} // namespace systems::leal::campello_widgets
