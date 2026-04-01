#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    class OverlayEntry;

    /**
     * @brief An action button placed inside a SnackBar.
     *
     * @code
     * auto action = std::make_shared<SnackBarAction>();
     * action->label      = "UNDO";
     * action->on_pressed = []{ undoLastAction(); };
     * @endcode
     */
    class SnackBarAction : public StatelessWidget
    {
    public:
        std::string           label;
        Color                 text_color = Color::fromRGBA(0.4f, 0.8f, 1.0f, 1.0f);
        std::function<void()> on_pressed;

        SnackBarAction() = default;

        WidgetRef build(BuildContext& ctx) const override;
    };

    /**
     * @brief A brief message bar displayed at the bottom of the screen.
     *
     * SnackBar appears at the bottom of the viewport and auto-dismisses after
     * `duration_ms` milliseconds. Use showSnackBar() to display it.
     *
     * @code
     * showSnackBar(std::make_shared<SnackBar>(
     *     std::make_shared<Text>("Item deleted")));
     * @endcode
     */
    class SnackBar : public StatelessWidget
    {
    public:
        WidgetRef   content;
        WidgetRef   action;
        Color       background_color = Color::fromRGBA(0.2f, 0.2f, 0.2f, 0.9f);
        float       border_radius    = 4.0f;
        double      duration_ms      = 4000.0;
        EdgeInsets  padding          = EdgeInsets::symmetric(16.0f, 14.0f);

        SnackBar() = default;
        explicit SnackBar(WidgetRef content_widget)
            : content(std::move(content_widget)) {}

        WidgetRef build(BuildContext& ctx) const override;
    };

    // -------------------------------------------------------------------------
    // showSnackBar / hideSnackBar
    // -------------------------------------------------------------------------

    /**
     * @brief Displays a SnackBar at the bottom of the screen.
     *
     * The bar slides up from the bottom and auto-dismisses after `duration_ms`.
     * Returns an OverlayEntry that can be passed to hideSnackBar() for early
     * dismissal.
     */
    std::shared_ptr<OverlayEntry> showSnackBar(
        WidgetRef snackbar,
        double    duration_ms = 4000.0);

    /**
     * @brief Dismisses the given SnackBar immediately.
     */
    void hideSnackBar(std::shared_ptr<OverlayEntry> entry);

} // namespace systems::leal::campello_widgets
