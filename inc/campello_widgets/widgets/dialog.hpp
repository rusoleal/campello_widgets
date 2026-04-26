#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class OverlayEntry;

    /**
     * @brief A container for a material design dialog.
     *
     * Dialog is a simple container with rounded corners, background color,
     * and elevation shadow. The content is provided by the child widget.
     *
     * For pre-built dialogs with title, content, and actions, use AlertDialog.
     *
     * Example:
     * @code
     * Dialog::create(
     *     Padding::create(EdgeInsets::all(24),
     *         Column::create({
     *             Text::create("Custom Dialog"),
     *             Text::create("Dialog content here"),
     *         })
     *     )
     * )
     * @endcode
     */
    class Dialog : public StatelessWidget
    {
    public:
        /** @brief The dialog's content. */
        WidgetRef child;

        /** @brief Background color. Falls back to Theme surface tokens when unset. */
        std::optional<Color> background_color;

        /** @brief Corner radius. */
        float border_radius = 8.0f;

        /** @brief Elevation shadow (0 for no shadow). */
        float elevation = 24.0f;

        /** @brief Minimum width. */
        float min_width = 280.0f;

        /** @brief Maximum width. */
        float max_width = 560.0f;

        Dialog() = default;
        explicit Dialog(WidgetRef c) : child(std::move(c)) {}

        WidgetRef build(BuildContext& context) const override;

        // Factory methods
        static std::shared_ptr<Dialog> create(WidgetRef child)
        {
            return std::make_shared<Dialog>(std::move(child));
        }

        static std::shared_ptr<Dialog> create(WidgetRef child, Color bg, float radius = 8.0f)
        {
            auto d = std::make_shared<Dialog>(std::move(child));
            d->background_color = bg;
            d->border_radius = radius;
            return d;
        }
    };

    /**
     * @brief A material design alert dialog.
     *
     * AlertDialog is a pre-built dialog with optional title, content,
     * and action buttons. It follows Material Design guidelines for
     * spacing, typography, and button placement.
     *
     * Example:
     * @code
     * AlertDialog::create(
     *     "Confirm Delete",
     *     "Are you sure you want to delete this item?",
     *     {
     *         AlertDialogAction::create("Cancel", [] {}),
     *         AlertDialogAction::create("Delete", Colors::red, [] { doDelete(); }),
     *     }
     * )
     * @endcode
     */
    class AlertDialog : public StatelessWidget
    {
    public:
        /** @brief Optional title text. */
        std::string title;

        /** @brief Optional content text or widget. */
        std::string content_text;
        WidgetRef content_widget;

        /** @brief Action buttons (typically TextButton). */
        std::vector<WidgetRef> actions;

        /** @brief Background color. Falls back to Theme surface tokens when unset. */
        std::optional<Color> background_color;

        AlertDialog() = default;

        WidgetRef build(BuildContext& context) const override;

        // Factory methods

        /**
         * @brief Creates an alert dialog with text content.
         */
        static std::shared_ptr<AlertDialog> create(
            const std::string& title,
            const std::string& content,
            std::vector<WidgetRef> actions)
        {
            auto d = std::make_shared<AlertDialog>();
            d->title = title;
            d->content_text = content;
            d->actions = std::move(actions);
            return d;
        }

        /**
         * @brief Creates an alert dialog with custom content widget.
         */
        static std::shared_ptr<AlertDialog> createWithWidget(
            const std::string& title,
            WidgetRef content,
            std::vector<WidgetRef> actions)
        {
            auto d = std::make_shared<AlertDialog>();
            d->title = title;
            d->content_widget = std::move(content);
            d->actions = std::move(actions);
            return d;
        }
    };

    /**
     * @brief A simple text button for use in dialogs.
     *
     * This is a convenience widget that creates a styled text button.
     * In a full implementation, this would be a separate TextButton widget.
     */
    class DialogAction : public StatelessWidget
    {
    public:
        std::string text;
        Color text_color = Color::fromRGB(0.051f, 0.545f, 0.553f);  // Material Blue
        std::function<void()> on_pressed;

        DialogAction() = default;
        DialogAction(std::string t, std::function<void()> on_press)
            : text(std::move(t))
            , on_pressed(std::move(on_press))
        {}
        DialogAction(std::string t, Color c, std::function<void()> on_press)
            : text(std::move(t))
            , text_color(c)
            , on_pressed(std::move(on_press))
        {}

        WidgetRef build(BuildContext& context) const override;

        // Factory
        static std::shared_ptr<DialogAction> create(
            std::string text,
            std::function<void()> on_press)
        {
            return std::make_shared<DialogAction>(std::move(text), std::move(on_press));
        }

        static std::shared_ptr<DialogAction> create(
            std::string text,
            Color color,
            std::function<void()> on_press)
        {
            return std::make_shared<DialogAction>(
                std::move(text), color, std::move(on_press));
        }
    };

    // -------------------------------------------------------------------------
    // showDialog helper
    // -------------------------------------------------------------------------

    /**
     * @brief Displays a dialog above the current contents.
     *
     * showDialog is the main entry point for displaying dialogs. It:
     * 1. Creates a ModalBarrier behind the dialog
     * 2. Centers the dialog on screen
     * 3. Handles dismiss on barrier tap
     * 4. Returns an OverlayEntry that can be used to close the dialog
     *
     * @param child The dialog widget to display (typically Dialog or AlertDialog)
     * @param barrier_dismissible Whether tapping outside dismisses the dialog
     * @param barrier_color Color of the barrier behind the dialog
     * @return The OverlayEntry containing the dialog, which can be removed to close it
     */
    std::shared_ptr<OverlayEntry> showDialog(
        WidgetRef child,
        bool barrier_dismissible = true,
        Color barrier_color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.5f));

    /**
     * @brief Closes the dialog containing the given context.
     *
     * This is a convenience function that removes the dialog's overlay entry.
     */
    void hideDialog(std::shared_ptr<OverlayEntry> entry);

} // namespace systems::leal::campello_widgets
