#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A circular floating action button (FAB).
     *
     * FloatingActionButton renders a round elevated button, typically used for
     * the primary action of a screen. Set `mini = true` for a smaller 40 dp
     * variant.
     *
     * When `on_pressed` is nullptr the button is rendered at 38% opacity.
     *
     * @code
     * auto fab = std::make_shared<FloatingActionButton>();
     * fab->child      = std::make_shared<Text>("+");
     * fab->on_pressed = []{ createItem(); };
     * @endcode
     */
    class FloatingActionButton : public StatelessWidget
    {
    public:
        WidgetRef             child;
        std::function<void()> on_pressed;
        Color                 background_color = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color                 foreground_color = Color::white();
        float                 elevation        = 6.0f;
        bool                  mini             = false;

        FloatingActionButton() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
