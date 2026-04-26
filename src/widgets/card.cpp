#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Card::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        CardConfig cfg;
        cfg.child    = child;
        cfg.priority = priority;
        cfg.padding  = padding;

        return ds->buildCard(cfg);
    }

} // namespace systems::leal::campello_widgets
