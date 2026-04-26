#include <campello_widgets/widgets/progress_indicator.hpp>
#include <campello_widgets/widgets/theme.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef ProgressIndicator::build(BuildContext& ctx) const
    {
        const auto* ds = Theme::of(ctx);
        if (!ds) return nullptr;
        return ds->buildProgressIndicator(ProgressConfig{type, value});
    }

} // namespace systems::leal::campello_widgets
