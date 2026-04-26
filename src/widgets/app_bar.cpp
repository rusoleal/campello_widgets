#include <campello_widgets/widgets/app_bar.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef AppBar::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        AppBarConfig cfg;
        cfg.title       = title;
        cfg.leading     = leading;
        cfg.actions     = actions;
        cfg.center_title = center_title;

        return ds->buildAppBar(cfg);
    }

} // namespace systems::leal::campello_widgets
