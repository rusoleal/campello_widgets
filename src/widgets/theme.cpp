#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/ui/campello_design_system.hpp>

namespace systems::leal::campello_widgets
{

    const DesignSystem* Theme::of(BuildContext& context)
    {
        const auto* widget = context.dependOnInheritedWidgetOfExactType<Theme>();
        if (widget && widget->design_system) {
            return widget->design_system.get();
        }
        static const auto default_ds = std::make_shared<CampelloDesignSystem>();
        return default_ds.get();
    }

    const DesignTokens* Theme::tokensOf(BuildContext& context)
    {
        const auto* ds = of(context);
        if (!ds) return nullptr;
        return &ds->tokens();
    }

    const TextStyle* Theme::textStyleOf(BuildContext& context, TextRole role)
    {
        const auto* tokens = tokensOf(context);
        if (!tokens) return nullptr;
        return &textStyleForRole(tokens->typography, role);
    }

    bool Theme::updateShouldNotify(const InheritedWidget& old_widget) const
    {
        const auto& old_theme = static_cast<const Theme&>(old_widget);
        return design_system != old_theme.design_system;
    }

} // namespace systems::leal::campello_widgets
