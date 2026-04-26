#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Divider::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;

        DividerConfig cfg;
        cfg.indent     = indent;
        cfg.end_indent = end_indent;

        return ds->buildDivider(cfg);
    }

} // namespace systems::leal::campello_widgets
