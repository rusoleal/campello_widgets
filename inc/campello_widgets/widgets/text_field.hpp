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
     * @brief A text input field supporting both single-line and multi-line modes.
     *
     * TextField is a controlled widget: text and selection state live in a
     * TextEditingController. If `controller` is null the TextField creates
     * and owns one internally. If `focus_node` is null, an internal FocusNode
     * is created and managed.
     *
     * ## Single-line mode (default)
     * - max_lines = 1 (default)
     * - Enter key triggers on_submitted
     *
     * ## Multi-line mode
     * - max_lines > 1 or max_lines = 0 (unlimited)
     * - Enter key inserts newlines
     * - on_submitted is called when Ctrl+Enter is pressed
     *
     * @code
     * // Single-line
     * auto ctrl = std::make_shared<TextEditingController>("Hello");
     * auto tf   = std::make_shared<TextField>();
     * tf->controller    = ctrl;
     * tf->placeholder   = "Type here…";
     * tf->on_submitted  = [](const std::string& v) { submit(v); };
     *
     * // Multi-line
     * auto multi = std::make_shared<TextField>();
     * multi->max_lines = 10;
     * multi->min_lines = 3;
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

        /** @brief Called when the user presses Enter (single-line) or Ctrl+Enter (multi-line). */
        std::function<void(const std::string&)> on_submitted;

        /** @brief Hint text shown when the field is empty. */
        std::string placeholder;

        /** @brief If true, draws bullets instead of typed characters. */
        bool obscure_text = false;

        /** @brief Text style (font size, color, weight, …). */
        TextStyle style;

        /**
         * @brief Maximum number of lines to show.
         *
         * - 1 (default): Single-line mode
         * - > 1: Multi-line mode with scroll after max_lines
         * - 0: Multi-line mode with unlimited lines (expands to content)
         */
        int max_lines = 1;

        /**
         * @brief Minimum number of lines to show.
         *
         * The field will be at least tall enough to show this many lines.
         * Only applies when max_lines != 1.
         */
        int min_lines = 1;

        /**
         * @brief If true and max_lines is unlimited (0), expands to fill parent.
         *
         * When true, the field takes all available vertical space.
         * When false (default), the field sizes to its content.
         */
        bool expands = false;

        std::optional<Color> fill_color;
        std::optional<Color> border_color;
        std::optional<Color> focused_border_color;
        std::optional<Color> cursor_color;
        std::optional<Color> selection_color;
        std::optional<Color> placeholder_color;

        float border_radius = 8.0f;
        float border_width  = 1.0f;
        float min_height    = 36.0f;

        TextField() = default;
        explicit TextField(std::string place_holder)
            : placeholder(std::move(place_holder))
        {}
        explicit TextField(
            std::string place_holder,
            std::function<void(const std::string&)> on_change)
            : on_changed(std::move(on_change))
            , placeholder(std::move(place_holder))
        {}
        explicit TextField(
            std::shared_ptr<TextEditingController> ctrl,
            std::string place_holder = "")
            : controller(std::move(ctrl))
            , placeholder(std::move(place_holder))
        {}

        /** @brief Returns true if this is a multi-line text field. */
        bool isMultiline() const { return max_lines != 1; }

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
