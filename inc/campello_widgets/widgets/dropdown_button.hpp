#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A single item in a DropdownButton menu.
     */
    template<typename T>
    struct DropdownMenuItem
    {
        T         value;
        WidgetRef child;
        bool      enabled = true;
    };

    // -------------------------------------------------------------------------
    // DropdownButton
    // -------------------------------------------------------------------------

    /**
     * @brief A button that displays a dropdown menu of selectable items.
     *
     * Because this is a template class the full implementation (including the
     * State) lives in this header.
     *
     * @code
     * auto dd = std::make_shared<DropdownButton<std::string>>();
     * dd->items = {
     *     {"Apple",  std::make_shared<Text>("Apple")},
     *     {"Banana", std::make_shared<Text>("Banana")},
     * };
     * dd->value      = "Apple";
     * dd->on_changed = [](std::string v){ selected = v; };
     * @endcode
     */
    template<typename T>
    class DropdownButton : public StatefulWidget
    {
    public:
        std::vector<DropdownMenuItem<T>> items;
        std::optional<T>                 value;
        std::function<void(T)>           on_changed;
        std::string                      hint;
        Color                            dropdown_color = Color::white();
        float                            border_radius  = 4.0f;
        float                            elevation      = 8.0f;

        DropdownButton() = default;
        explicit DropdownButton(std::vector<DropdownMenuItem<T>> itms)
            : items(std::move(itms))
        {}
        explicit DropdownButton(
            std::vector<DropdownMenuItem<T>> itms,
            T val,
            std::function<void(T)> on_change)
            : items(std::move(itms)), value(val), on_changed(std::move(on_change))
        {}

        // ------------------------------------------------------------------
        // State
        // ------------------------------------------------------------------

        class DropdownButtonState : public State<DropdownButton<T>>
        {
        public:
            void dispose() override { close(); }

            WidgetRef build(BuildContext&) override
            {
                const auto& w = this->widget();

                // Selected label (or hint)
                WidgetRef label;
                if (w.value.has_value()) {
                    for (const auto& item : w.items) {
                        if (item.value == *w.value) {
                            label = item.child;
                            break;
                        }
                    }
                }
                if (!label) {
                    TextStyle ts;
                    ts.color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.54f);
                    label = std::make_shared<Text>(w.hint, ts);
                }

                // Chevron glyph
                auto arrow = std::make_shared<Text>(
                    "\u25BC",
                    TextStyle{}.withFontSize(10.0f)
                               .withColor(Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.54f)));

                auto row = std::make_shared<Row>();
                row->main_axis_alignment = MainAxisAlignment::spaceBetween;
                row->cross_axis_alignment = CrossAxisAlignment::center;

                row->children = {std::make_shared<Expanded>(label), arrow};

                auto padded = std::make_shared<Padding>();
                padded->padding = EdgeInsets::symmetric(12.0f, 8.0f);
                padded->child   = row;

                BoxDecoration deco;
                deco.color         = Color::white();
                deco.border_radius = w.border_radius;
                deco.border = BoxBorder::all(
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.38f), 1.0f);

                auto decorated = std::make_shared<DecoratedBox>();
                decorated->decoration = deco;
                decorated->child      = padded;

                auto gesture = std::make_shared<GestureDetector>();
                gesture->on_tap = [this]() { open(); };
                gesture->child  = decorated;

                return gesture;
            }

        private:
            std::shared_ptr<OverlayEntry> barrier_entry_;
            std::shared_ptr<OverlayEntry> menu_entry_;

            void open()
            {
                if (menu_entry_) return;
                const auto& w = this->widget();

                // Build menu items
                std::vector<WidgetRef> item_widgets;
                int idx = 0;
                for (const auto& item : w.items) {
                    int i = idx++;
                    WidgetRef item_child = item.child;
                    auto padded = std::make_shared<Padding>();
                    padded->padding = EdgeInsets::symmetric(12.0f, 10.0f);
                    padded->child   = item_child;

                    if (item.enabled) {
                        auto g  = std::make_shared<GestureDetector>();
                        g->on_tap = [this, i, val = item.value]() {
                            close();
                            const auto& ww = this->widget();
                            if (ww.on_changed) ww.on_changed(val);
                        };
                        g->child = padded;
                        item_widgets.push_back(g);
                    } else {
                        auto faded     = std::make_shared<Opacity>();
                        faded->opacity = 0.38f;
                        faded->child   = padded;
                        item_widgets.push_back(faded);
                    }
                }

                auto col = std::make_shared<Column>();
                col->main_axis_size = MainAxisSize::min;
                col->cross_axis_alignment = CrossAxisAlignment::stretch;
                col->children = std::move(item_widgets);

                BoxDecoration menu_deco;
                menu_deco.color         = w.dropdown_color;
                menu_deco.border_radius = w.border_radius;
                if (w.elevation > 0.0f) {
                    menu_deco.box_shadow = {BoxShadow{
                        Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.3f),
                        Offset{0.0f, w.elevation * 0.5f},
                        w.elevation * 2.0f
                    }};
                }

                auto menu_box = std::make_shared<DecoratedBox>();
                menu_box->decoration = menu_deco;
                menu_box->child      = col;

                // Center the menu on screen
                auto aligned = std::make_shared<Align>();
                aligned->alignment = Alignment::center();
                aligned->child     = menu_box;

                // Barrier + menu in a stack
                auto dismiss = [this]() { close(); };
                auto barrier = ModalBarrier::create(
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f), true, dismiss);

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

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<DropdownButtonState>();
        }
    };

} // namespace systems::leal::campello_widgets
