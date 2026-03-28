#include <campello_widgets/widgets/dialog.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Dialog
    // -------------------------------------------------------------------------

    WidgetRef Dialog::build(BuildContext& context) const
    {
        (void)context;

        if (!child) return nullptr;

        // Container with background, padding, and rounded corners
        auto container = std::make_shared<Container>();
        container->child = child;
        container->color = background_color;
        // Note: border_radius and elevation would need Container decoration support
        // For now, just use the basic container

        // Constrain width
        auto constrained = std::make_shared<SizedBox>();
        constrained->width = max_width;
        constrained->child = container;

        return constrained;
    }

    // -------------------------------------------------------------------------
    // AlertDialog
    // -------------------------------------------------------------------------

    WidgetRef AlertDialog::build(BuildContext& context) const
    {
        (void)context;

        std::vector<WidgetRef> dialog_children;

        // Title
        if (!title.empty()) {
            auto title_text = std::make_shared<Text>(
                title,
                TextStyle{}
                    .withFontSize(20.0f)
                    .bold()
                    .withColor(Color::black()));
            dialog_children.push_back(
                Padding::create(EdgeInsets::only(24, 24, 24, 0), title_text));
        }

        // Content
        if (content_widget) {
            dialog_children.push_back(
                Padding::create(EdgeInsets::only(24, 16, 24, 0), content_widget));
        } else if (!content_text.empty()) {
            auto content = std::make_shared<Text>(
                content_text,
                TextStyle{}
                    .withFontSize(16.0f)
                    .withColor(Color::fromRGB(0.4f, 0.4f, 0.4f)));
            dialog_children.push_back(
                Padding::create(EdgeInsets::only(24, 16, 24, 0), content));
        }

        // Spacer between content and actions
        if (!actions.empty()) {
            dialog_children.push_back(SizedBox::create(0, 16));

            // Actions row (aligned to end)
            WidgetRef actions_row;
            if (actions.size() == 1) {
                actions_row = Align::create(
                    Alignment::centerRight(),
                    Padding::create(EdgeInsets::only(8, 0, 8, 8), actions[0]));
            } else {
                auto row = std::make_shared<Row>();
                row->main_axis_alignment = MainAxisAlignment::end;
                row->children = actions;
                actions_row = Padding::create(EdgeInsets::only(8, 0, 8, 8), row);
            }
            dialog_children.push_back(actions_row);
        } else {
            // Bottom padding if no actions
            dialog_children.push_back(SizedBox::create(0, 16));
        }

        // Build dialog content column
        auto content_column = Column::create(dialog_children);
        content_column->main_axis_size = MainAxisSize::min;
        content_column->cross_axis_alignment = CrossAxisAlignment::stretch;

        // Wrap in Dialog container
        return Dialog::create(content_column);
    }

    // -------------------------------------------------------------------------
    // DialogAction
    // -------------------------------------------------------------------------

    WidgetRef DialogAction::build(BuildContext& context) const
    {
        (void)context;

        auto text_widget = std::make_shared<Text>(
            text,
            TextStyle{}
                .withColor(text_color)
                .withFontSize(14.0f)
                .bold());

        // Wrap in padding for touch target
        auto padded = Padding::create(EdgeInsets::all(12), text_widget);

        // Add tap handler
        if (on_pressed) {
            auto gesture = std::make_shared<GestureDetector>();
            gesture->child = padded;
            gesture->on_tap = on_pressed;
            return gesture;
        }

        return padded;
    }

    // -------------------------------------------------------------------------
    // showDialog / hideDialog
    // -------------------------------------------------------------------------

    std::shared_ptr<OverlayEntry> showDialog(
        WidgetRef child,
        bool barrier_dismissible,
        Color barrier_color)
    {
        if (!child) return nullptr;

        // Create the overlay entry that will contain both barrier and dialog
        auto entry = std::make_shared<OverlayEntry>();

        // Create dismiss callback
        auto dismiss = [entry]() {
            Overlay::remove(entry);
        };

        // Set on_remove callback
        entry->on_remove = nullptr;  // Could add callback here

        // Build the dialog stack: barrier + centered dialog
        std::vector<WidgetRef> stack_children;

        // Modal barrier (full screen, behind dialog)
        stack_children.push_back(
            ModalBarrier::create(barrier_color, barrier_dismissible, dismiss));

        // Centered dialog (on top)
        stack_children.push_back(
            Center::create(
                Padding::create(EdgeInsets::all(40), child)));

        auto stack = Stack::create(stack_children);
        stack->fit = StackFit::expand;

        entry->child = stack;
        entry->opaque = true;  // Block interaction with content below

        // Insert into overlay
        Overlay::insert(entry);

        return entry;
    }

    void hideDialog(std::shared_ptr<OverlayEntry> entry)
    {
        if (entry) {
            Overlay::remove(entry);
        }
    }

} // namespace systems::leal::campello_widgets
