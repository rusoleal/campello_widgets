#pragma once

#include <campello_widgets/ui/design_system.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Propagates a DesignSystem down the widget tree.
     *
     * Adaptive widgets (Button, Switch, Card, etc.) call
     * Theme::of(context) to obtain the active DesignSystem and delegate
     * their build to it.
     *
     * Because Theme is an InheritedWidget, changing the design_system
     * pointer automatically rebuilds all dependent widgets, enabling
     * runtime switching between light/dark modes or even entirely
     * different design systems (e.g. Material → Cupertino → LiquidGlass).
     *
     * @code
     * runApp(mw<Theme>(Theme{
     *     .design_system = CampelloDesignSystem::light(),
     *     .child         = mw<MyApp>(),
     * }));
     * @endcode
     */
    class Theme : public InheritedWidget
    {
    public:
        std::shared_ptr<const DesignSystem> design_system;

        Theme(std::shared_ptr<const DesignSystem> ds, WidgetRef child)
            : design_system(std::move(ds))
        {
            this->child = std::move(child);
        }

        /**
         * @brief Looks up the nearest Theme ancestor and registers this
         *        element as a dependent so it rebuilds when the theme changes.
         *
         * @return Pointer to the DesignSystem, or nullptr if no Theme is found.
         */
        static const DesignSystem* of(BuildContext& context);

        /**
         * @brief Convenience accessor for the raw tokens of the active design system.
         *
         * @return Pointer to the tokens, or nullptr if no Theme is found.
         */
        static const DesignTokens* tokensOf(BuildContext& context);

        /**
         * @brief Convenience accessor for a text style from the active design system.
         *
         * @return Pointer to the TextStyle, or nullptr if no Theme is found.
         */
        static const TextStyle* textStyleOf(BuildContext& context, TextRole role);

        bool updateShouldNotify(const InheritedWidget& old_widget) const override;
    };

} // namespace systems::leal::campello_widgets
