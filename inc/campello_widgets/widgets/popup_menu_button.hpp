#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Describes a single item in a PopupMenuButton.
     *
     * Set `is_divider = true` to render a horizontal separator instead of a
     * selectable row.
     */
    struct PopupMenuItem
    {
        std::string           label;
        WidgetRef             child;         ///< If set, overrides label.
        bool                  enabled    = true;
        bool                  is_divider = false;
        std::function<void()> on_tap;
    };

    /**
     * @brief A button that shows a contextual popup menu when tapped.
     *
     * By default the button renders a vertical three-dot icon. Provide a
     * `child` to customise the trigger widget.
     *
     * @code
     * auto menu = std::make_shared<PopupMenuButton>();
     * menu->items = {
     *     PopupMenuItem{"Edit",   {}, true, false, []{ edit();   }},
     *     PopupMenuItem{"Delete", {}, true, false, []{ remove(); }},
     * };
     * @endcode
     */
    class PopupMenuButton : public StatefulWidget
    {
    public:
        WidgetRef                      child;
        std::vector<PopupMenuItem>     items;
        std::function<void(int index)> on_selected;
        std::optional<Color>           popup_color;
        float                          border_radius = 8.0f;
        float                          elevation     = 8.0f;

        PopupMenuButton() = default;
        explicit PopupMenuButton(std::vector<PopupMenuItem> itms)
            : items(std::move(itms))
        {}
        explicit PopupMenuButton(
            WidgetRef c,
            std::vector<PopupMenuItem> itms)
            : child(std::move(c)), items(std::move(itms))
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
