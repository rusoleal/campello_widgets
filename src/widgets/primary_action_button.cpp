#include <campello_widgets/widgets/primary_action_button.hpp>
#include <campello_widgets/widgets/theme.hpp>

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

        return ds->buildPrimaryActionButton(cfg);
    }

} // namespace systems::leal::campello_widgets
