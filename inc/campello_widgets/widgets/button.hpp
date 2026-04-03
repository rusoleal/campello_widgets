#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A basic interactive button widget.
     *
     * Wraps its `child` in a rounded, coloured surface that responds to taps.
     * Set `on_pressed` to nullptr to make the button inactive (rendered at
     * reduced opacity).
     *
     * @code
     * auto btn = std::make_shared<Button>();
     * btn->child      = std::make_shared<Text>("Click me");
     * btn->on_pressed = []() { }; // handle tap
     *
     * // Or use Text directly as child for a simple label button:
     * btn->background_color = Color::fromRGB(0.2f, 0.6f, 1.0f);
     * @endcode
     */
    class Button : public StatelessWidget
    {
    public:
        WidgetRef              child;
        std::function<void()>  on_pressed;

        Color       background_color = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color       foreground_color = Color::white();
        EdgeInsets  padding          = EdgeInsets::symmetric(16.0f, 8.0f);
        float       border_radius    = 4.0f;
        float       elevation        = 2.0f;

        Button() = default;
        explicit Button(WidgetRef c, std::function<void()> on_press = nullptr)
            : child(std::move(c)), on_pressed(std::move(on_press))
        {}
        explicit Button(WidgetRef c, std::function<void()> on_press, Color bg)
            : child(std::move(c)), on_pressed(std::move(on_press)), background_color(bg)
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
