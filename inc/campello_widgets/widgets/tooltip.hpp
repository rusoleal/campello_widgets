#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Shows a text label when the user long-presses the child widget.
     *
     * The tooltip appears as a floating dark rounded box above (or below) the
     * child and auto-dismisses after `display_duration_ms` milliseconds.
     *
     * @code
     * auto t = std::make_shared<Tooltip>();
     * t->message = "Save file";
     * t->child   = std::make_shared<Button>(...);
     * @endcode
     */
    class Tooltip : public StatefulWidget
    {
    public:
        WidgetRef   child;
        std::string message;

        /// How long to keep the tooltip visible after it appears.
        double display_duration_ms = 2000.0;

        Color      background_color = Color::fromRGBA(0.1f, 0.1f, 0.1f, 0.9f);
        Color      text_color       = Color::white();
        float      border_radius    = 4.0f;
        EdgeInsets padding          = EdgeInsets::symmetric(8.0f, 4.0f);
        float      font_size        = 12.0f;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
