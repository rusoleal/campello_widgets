#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Button::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        ButtonConfig cfg;
        cfg.label         = child;
        cfg.on_pressed    = on_pressed;
        cfg.priority      = priority;
        cfg.enabled       = enabled;
        cfg.leading_icon  = leading_icon;
        cfg.trailing_icon = trailing_icon;

        return ds->buildButton(cfg);
    }

} // namespace systems::leal::campello_widgets
