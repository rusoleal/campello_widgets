#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef ListTile::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        ListTileConfig cfg;
        cfg.leading     = leading;
        cfg.title       = title;
        cfg.subtitle    = subtitle;
        cfg.trailing    = trailing;
        cfg.on_tap      = on_tap;
        cfg.enabled     = enabled;

        return ds->buildListTile(cfg);
    }

} // namespace systems::leal::campello_widgets
