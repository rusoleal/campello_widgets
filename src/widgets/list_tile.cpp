#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>

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

        auto rendered = ds->buildListTile(cfg);
        if (!rendered) return nullptr;

        // Only a pointer cursor when the tile actually does something --
        // a display-only ListTile (no on_tap) should look like plain
        // content, not a clickable row.
        if (!on_tap || !enabled) return rendered;

        auto region    = std::make_shared<MouseRegion>();
        region->cursor = SystemMouseCursor::pointer;
        region->child  = rendered;
        return region;
    }

} // namespace systems::leal::campello_widgets
