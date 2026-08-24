#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>

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

        auto rendered = ds->buildButton(cfg);
        if (!rendered) return nullptr;

        // Wrapped once here rather than per DesignSystem -- every
        // buildButton() implementation (Material/Cupertino/Fluent/UI)
        // funnels through this same Button::build(), so the hover cursor
        // stays uniform across themes without touching any of them.
        auto region    = std::make_shared<MouseRegion>();
        region->cursor = enabled ? SystemMouseCursor::pointer : SystemMouseCursor::arrow;
        region->child  = rendered;
        return region;
    }

} // namespace systems::leal::campello_widgets
