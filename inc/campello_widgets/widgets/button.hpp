#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive button that delegates its visual appearance to the
     *        active DesignSystem.
     *
     * The button's look (colors, padding, radius, elevation) is decided by the
     * current theme's DesignSystem based on the semantic `priority` you supply.
     *
     * @code
     * auto btn = std::make_shared<Button>(
     *     std::make_shared<Text>("Save"),
     *     []() { save(); });
     * btn->priority = ButtonPriority::primary;
     * @endcode
     */
    class Button : public StatelessWidget
    {
    public:
        WidgetRef              child;
        std::function<void()>  on_pressed;
        ButtonPriority         priority = ButtonPriority::primary;
        bool                   enabled  = true;
        WidgetRef              leading_icon;
        WidgetRef              trailing_icon;

        Button() = default;
        explicit Button(WidgetRef c, std::function<void()> on_press = nullptr)
            : child(std::move(c)), on_pressed(std::move(on_press))
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
