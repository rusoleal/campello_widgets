#include <campello_widgets/widgets/snack_bar.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/text_style.hpp>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // SnackBarAction
    // -------------------------------------------------------------------------

    WidgetRef SnackBarAction::build(BuildContext&) const
    {
        TextStyle ts;
        ts.color     = text_color;
        ts.font_size = 14.0f;

        auto text   = std::make_shared<Text>(label, ts);
        auto padded = std::make_shared<Padding>();
        padded->padding = EdgeInsets::symmetric(0.0f, 8.0f);
        padded->child   = text;

        if (on_pressed) {
            auto g    = std::make_shared<GestureDetector>();
            g->on_tap = on_pressed;
            g->child  = padded;
            return g;
        }
        return padded;
    }

    // -------------------------------------------------------------------------
    // SnackBar
    // -------------------------------------------------------------------------

    WidgetRef SnackBar::build(BuildContext&) const
    {
        std::vector<WidgetRef> row_children;
        row_children.push_back(std::make_shared<Expanded>(content));
        if (action) row_children.push_back(action);

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        auto padded     = std::make_shared<Padding>();
        padded->padding = padding;
        padded->child   = row;

        BoxDecoration deco;
        deco.color         = background_color;
        deco.border_radius = border_radius;

        auto decorated        = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        return decorated;
    }

    // -------------------------------------------------------------------------
    // showSnackBar / hideSnackBar
    // -------------------------------------------------------------------------

    std::shared_ptr<OverlayEntry> showSnackBar(
        WidgetRef snackbar,
        double    duration_ms)
    {
        if (!snackbar) return nullptr;

        // Position at the bottom of the screen with margins
        auto padded     = std::make_shared<Padding>();
        padded->padding = EdgeInsets::only(16.0f, 0.0f, 16.0f, 16.0f);
        padded->child   = snackbar;

        auto aligned       = std::make_shared<Align>();
        aligned->alignment = Alignment::bottomCenter();
        aligned->child     = padded;

        auto entry = std::make_shared<OverlayEntry>(aligned);

        // Auto-dismiss timer.
        // Use weak_ptrs to avoid cycles: entry -> on_remove -> ctrl
        //                                ctrl -> listener -> entry (weak)
        auto ctrl            = std::make_shared<AnimationController>(duration_ms);
        auto entry_weak      = std::weak_ptr<OverlayEntry>(entry);
        auto ctrl_weak       = std::weak_ptr<AnimationController>(ctrl);

        ctrl->addListener([entry_weak, ctrl_weak]() {
            auto c = ctrl_weak.lock();
            if (!c) return;
            if (c->normalizedValue() >= 1.0) {
                if (auto e = entry_weak.lock()) {
                    Overlay::remove(e);
                }
            }
        });

        // Keep the controller alive for as long as the entry is in the overlay.
        entry->on_remove = [ctrl]() {};

        Overlay::insert(entry);
        ctrl->forward(0.0);

        return entry;
    }

    void hideSnackBar(std::shared_ptr<OverlayEntry> entry)
    {
        if (entry) Overlay::remove(entry);
    }

} // namespace systems::leal::campello_widgets
