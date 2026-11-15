#include <campello_widgets/widgets/toolbar.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Toolbar::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        ToolbarConfig cfg;
        cfg.groups    = groups;
        cfg.color     = color;
        cfg.direction = direction;
        cfg.floating  = floating;
        return ds->buildToolbar(cfg);
    }

} // namespace systems::leal::campello_widgets
