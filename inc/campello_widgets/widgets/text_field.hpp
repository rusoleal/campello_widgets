#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    class FocusNode;

    /**
     * @brief A single-line text input field.
     *
     * TextField is a controlled widget: text and selection state live in a
     * TextEditingController. If `controller` is null the TextField creates
     * and owns one internally. If `focus_node` is null, an internal FocusNode
     * is created and managed.
     *
     * @code
     * auto ctrl = std::make_shared<TextEditingController>("Hello");
     * auto tf   = std::make_shared<TextField>();
     * tf->controller    = ctrl;
     * tf->placeholder   = "Type here…";
     * tf->on_submitted  = [](const std::string& v) { submit(v); };
     * @endcode
     */
    class TextField : public StatefulWidget
    {
    public:
        /** @brief Optional external controller (null → internal one is created). */
        std::shared_ptr<TextEditingController> controller;

        /** @brief Optional external FocusNode (null → internal one is created). */
        std::shared_ptr<FocusNode> focus_node;

        /** @brief Called each time the text changes. */
        std::function<void(const std::string&)> on_changed;

        /** @brief Called when the user presses Enter. */
        std::function<void(const std::string&)> on_submitted;

        /** @brief Hint text shown when the field is empty. */
        std::string placeholder;

        /** @brief If true, draws bullets instead of typed characters. */
        bool obscure_text = false;

        /** @brief Text style (font size, color, weight, …). */
        TextStyle style;

        Color fill_color           = Color::white();
        Color border_color         = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.38f);
        Color focused_border_color = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color cursor_color         = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color selection_color      = Color::fromRGBA(0.098f, 0.463f, 0.824f, 0.4f);
        Color placeholder_color    = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.38f);

        float border_radius = 4.0f;
        float border_width  = 1.0f;
        float min_height    = 36.0f;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
