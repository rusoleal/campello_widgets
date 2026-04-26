#include <campello_widgets/widgets/popup_menu_button.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // PopupMenuButtonState
    // -------------------------------------------------------------------------

    class PopupMenuButtonState : public State<PopupMenuButton>
    {
    public:
        void dispose() override { close(); }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            WidgetRef trigger = w.child;
            if (!trigger) {
                // Default three-dot icon using text glyphs
                auto padded = std::make_shared<Padding>();
                padded->padding = EdgeInsets::all(8.0f);
                padded->child   = std::make_shared<Text>(
                    "\u22EE",
                    TextStyle{}.withFontSize(20.0f)
                               .withColor(Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.54f)));
                trigger = padded;
            }

            auto gesture    = std::make_shared<GestureDetector>();
            gesture->on_tap = [this]() { open(); };
            gesture->child  = trigger;
            return gesture;
        }

    private:
        std::shared_ptr<OverlayEntry> menu_entry_;

        void open()
        {
            if (menu_entry_) return;
            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(*element());
            const Color popup_bg = w.popup_color.value_or(tokens->colors.surface);

            // Build item rows
            std::vector<WidgetRef> item_widgets;
            int idx = 0;
            for (const auto& item : w.items) {
                int i = idx++;

                if (item.is_divider) {
                    item_widgets.push_back(std::make_shared<Divider>());
                    continue;
                }

                WidgetRef label = item.child;
                if (!label) {
                    label = std::make_shared<Text>(
                        item.label,
                        TextStyle{}.withFontSize(14.0f).withColor(Color::black()));
                }

                auto padded = std::make_shared<Padding>();
                padded->padding = EdgeInsets::symmetric(12.0f, 10.0f);
                padded->child   = label;

                WidgetRef row_widget;
                if (item.enabled) {
                    auto tap_fn = item.on_tap;
                    auto selected_fn = w.on_selected;
                    auto g    = std::make_shared<GestureDetector>();
                    g->on_tap = [this, i, tap_fn, selected_fn]() {
                        close();
                        if (tap_fn)      tap_fn();
                        if (selected_fn) selected_fn(i);
                    };
                    g->child  = padded;
                    row_widget = g;
                } else {
                    auto faded     = std::make_shared<Opacity>();
                    faded->opacity = 0.40f;
                    faded->child   = padded;
                    row_widget     = faded;
                }

                item_widgets.push_back(row_widget);
            }

            auto col = std::make_shared<Column>();
            col->main_axis_size       = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::stretch;
            col->children             = std::move(item_widgets);

            BoxDecoration menu_deco;
            menu_deco.color         = popup_bg;
            menu_deco.border_radius = w.border_radius;
            if (w.elevation > 0.0f) {
                menu_deco.box_shadow = {BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                    Offset{0.0f, w.elevation * 0.5f},
                    w.elevation * 2.0f
                }};
            }

            auto menu_box        = std::make_shared<DecoratedBox>();
            menu_box->decoration = menu_deco;
            menu_box->child      = col;

            auto aligned       = std::make_shared<Align>();
            aligned->alignment = Alignment::topRight();
            aligned->child     = menu_box;

            // Transparent barrier for dismissal
            auto dismiss = [this]() { close(); };
            auto barrier = ModalBarrier::create(
                Color::transparent(), true, dismiss);

            std::vector<WidgetRef> stack_children = {barrier, aligned};
            auto stack = Stack::create(stack_children);
            stack->fit = StackFit::expand;

            menu_entry_ = std::make_shared<OverlayEntry>(stack);
            Overlay::insert(menu_entry_);
        }

        void close()
        {
            if (menu_entry_) {
                Overlay::remove(menu_entry_);
                menu_entry_.reset();
            }
        }
    };

    // -------------------------------------------------------------------------

    std::unique_ptr<StateBase> PopupMenuButton::createState() const
    {
        return std::make_unique<PopupMenuButtonState>();
    }

} // namespace systems::leal::campello_widgets
