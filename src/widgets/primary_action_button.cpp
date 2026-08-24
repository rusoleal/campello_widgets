#include <campello_widgets/widgets/primary_action_button.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef PrimaryActionButton::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        PrimaryActionConfig cfg;
        cfg.on_pressed = on_pressed;
        cfg.icon       = icon;
        cfg.label      = label;
        cfg.enabled    = enabled;

        auto rendered = ds->buildPrimaryActionButton(cfg);
        if (!rendered) return nullptr;

        auto region    = std::make_shared<MouseRegion>();
        region->cursor = enabled ? SystemMouseCursor::pointer : SystemMouseCursor::arrow;
        region->child  = rendered;
        return region;
    }

} // namespace systems::leal::campello_widgets
