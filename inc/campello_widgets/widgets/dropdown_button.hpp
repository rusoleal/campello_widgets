#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/backdrop_filter.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_dropdown_menu_positioner.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/key.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    namespace detail
    {
        /**
         * @brief Widget counterpart of RenderDropdownMenuPositioner —
         * internal to DropdownButton, not part of the public API.
         */
        class DropdownMenuPositionerWidget : public SingleChildRenderObjectWidget
        {
        public:
            Offset anchor_pos;
            Size   anchor_size;

            std::shared_ptr<RenderObject> createRenderObject() const override
            {
                auto r = std::make_shared<RenderDropdownMenuPositioner>();
                r->anchor_pos  = anchor_pos;
                r->anchor_size = anchor_size;
                return r;
            }

            void updateRenderObject(RenderObject& ro) const override
            {
                auto& r = static_cast<RenderDropdownMenuPositioner&>(ro);
                r.anchor_pos  = anchor_pos;
                r.anchor_size = anchor_size;
                r.markNeedsLayout();
            }
        };
    }

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
        std::optional<Color>             dropdown_color;
        float                            border_radius = 8.0f;
        float                            elevation      = 8.0f;

        /**
         * @brief Padding around the closed trigger's label+chevron row.
         *
         * Default matches Material's normal-density trigger. Override for a
         * more compact trigger when embedding in a space with a fixed,
         * limited height (e.g. a toolbar) -- the default's 12px vertical
         * inset alone, plus the label's own line height, naturally wants
         * ~32-34px, taller than a typical compact toolbar row.
         */
        EdgeInsets                       content_padding = EdgeInsets::symmetric(12.0f, 8.0f);

        /**
         * @brief When set, the dropdown menu renders as a frosted/liquid-
         * glass panel — see `PopupMenuButton::backdrop_filter`'s doc for the
         * same mechanism.
         */
        std::optional<ImageFilter>       backdrop_filter;

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
            void initState() override
            {
                anchor_key_ = std::make_shared<GlobalKey>();
            }

            void dispose() override { close(); }

            WidgetRef build(BuildContext& ctx) override
            {
                const auto& w = this->widget();
                const auto* tokens = Theme::tokensOf(ctx);
                const Color outline = tokens->colors.outline;
                const Color on_surface_variant = tokens->colors.on_surface_variant;
                resolved_dropdown_color_ = w.dropdown_color.value_or(tokens->colors.surface);

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
                    ts.color = on_surface_variant;
                    label = std::make_shared<Text>(w.hint, ts);
                }

                // Chevron glyph
                auto arrow = std::make_shared<Text>(
                    "\u25BC",
                    TextStyle{}.withFontSize(10.0f)
                               .withColor(on_surface_variant));

                auto row = std::make_shared<Row>();
                row->main_axis_alignment = MainAxisAlignment::spaceBetween;
                row->cross_axis_alignment = CrossAxisAlignment::center;

                row->children = {std::make_shared<Expanded>(label), arrow};

                auto padded = std::make_shared<Padding>();
                padded->padding = w.content_padding;
                padded->child   = row;

                // Real M3 ExposedDropdownMenuBox's closed trigger is an
                // outlined field — border only, no fill — letting whatever
                // is behind it show through. Confirmed against a real
                // capture: the trigger box was fully transparent, not
                // filled with the surface color previously painted here.
                BoxDecoration deco;
                deco.border_radius = w.border_radius;
                deco.border = BoxBorder::all(outline, 1.0f);

                auto decorated = std::make_shared<DecoratedBox>();
                decorated->decoration = deco;
                decorated->child      = padded;

                auto gesture = std::make_shared<GestureDetector>();
                gesture->key    = anchor_key_;
                gesture->on_tap = [this]() { open(); };
                gesture->child  = decorated;

                auto region    = std::make_shared<MouseRegion>();
                region->cursor = SystemMouseCursor::pointer;
                region->child  = gesture;
                return region;
            }

        private:
            std::shared_ptr<OverlayEntry> barrier_entry_;
            std::shared_ptr<OverlayEntry> menu_entry_;
            std::shared_ptr<GlobalKey>    anchor_key_;
            Color resolved_dropdown_color_ = Color::white();

            void open()
            {
                if (menu_entry_) return;
                const auto& w = this->widget();
                const Color dropdown_bg = resolved_dropdown_color_;

                // Locate the button's own on-screen position/size so the
                // menu can open anchored to it, like Flutter's real
                // DropdownButton, instead of centered on screen. Looked up
                // fresh every time open() runs (not cached), so it's
                // always correct regardless of window size/resizes.
                Offset anchor_pos;
                Size   anchor_size = Size::zero();
                if (anchor_key_) {
                    if (auto* element = anchor_key_->currentElement()) {
                        if (auto* roe = element->findDescendantRenderObjectElement()) {
                            auto box = std::dynamic_pointer_cast<RenderGestureDetector>(
                                roe->sharedRenderObject());
                            if (box) {
                                anchor_pos  = box->globalOffset();
                                anchor_size = box->size();
                            }
                        }
                    }
                }

                // Build menu items
                std::vector<WidgetRef> item_widgets;
                for (const auto& item : w.items) {
                    WidgetRef item_child = item.child;
                    auto padded = std::make_shared<Padding>();
                    padded->padding = EdgeInsets::symmetric(12.0f, 10.0f);
                    padded->child   = item_child;

                    if (item.enabled) {
                        auto g  = std::make_shared<GestureDetector>();
                        g->on_tap = [this, val = item.value]() {
                            close();
                            const auto& ww = this->widget();
                            if (ww.on_changed) ww.on_changed(val);
                        };
                        g->child = padded;

                        auto item_region    = std::make_shared<MouseRegion>();
                        item_region->cursor = SystemMouseCursor::pointer;
                        item_region->child  = g;
                        item_widgets.push_back(item_region);
                    } else {
                        auto faded     = std::make_shared<Opacity>();
                        faded->opacity = 0.40f;
                        faded->child   = padded;
                        item_widgets.push_back(faded);
                    }
                }

                auto col = std::make_shared<Column>();
                col->main_axis_size = MainAxisSize::min;
                // start (not stretch): the menu sits inside a Positioned
                // child of a Stack::expand, which hands it a very wide
                // (effectively full-screen) max-width constraint. stretch
                // would make the column claim that entire width for itself
                // instead of sizing to its widest item, making the menu
                // span edge-to-edge.
                col->cross_axis_alignment = CrossAxisAlignment::start;
                col->children = std::move(item_widgets);

                WidgetRef menu_box;
                if (w.backdrop_filter.has_value()) {
                    // See PopupMenuButton::open()'s identical composition
                    // for why the shadow needs its own DecoratedBox wrapping
                    // the BackdropFilter rather than a flat fill.
                    auto bf    = std::make_shared<BackdropFilter>();
                    bf->filter = *w.backdrop_filter;
                    bf->child  = col;

                    BoxDecoration shadow_deco;
                    shadow_deco.border_radius = w.border_radius;
                    if (w.elevation > 0.0f) {
                        shadow_deco.box_shadow = {BoxShadow{
                            Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                            Offset{0.0f, w.elevation * 0.5f},
                            w.elevation * 2.0f
                        }};
                    }
                    auto shadowed        = std::make_shared<DecoratedBox>();
                    shadowed->decoration = shadow_deco;
                    shadowed->child      = bf;
                    menu_box = shadowed;
                } else {
                    BoxDecoration menu_deco;
                    menu_deco.color         = dropdown_bg;
                    menu_deco.border_radius = w.border_radius;
                    if (w.elevation > 0.0f) {
                        menu_deco.box_shadow = {BoxShadow{
                            Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
                            Offset{0.0f, w.elevation * 0.5f},
                            w.elevation * 2.0f
                        }};
                    }
                    auto decorated_box        = std::make_shared<DecoratedBox>();
                    decorated_box->decoration = menu_deco;
                    decorated_box->child      = col;
                    menu_box = decorated_box;
                }

                // Anchor the menu to the button, left edges aligned — like
                // Flutter's real DropdownButton, not centered on screen.
                // RenderDropdownMenuPositioner measures menu_box's actual
                // laid-out size during its own layout (it fills the Stack,
                // so its constraints equal the live viewport) and decides
                // above-vs-below from that real number, instead of a
                // pre-computed guess — see its doc comment.
                auto positioner = std::make_shared<detail::DropdownMenuPositionerWidget>();
                positioner->anchor_pos  = anchor_pos;
                positioner->anchor_size = anchor_size;
                positioner->child       = menu_box;

                // Barrier + menu in a stack
                auto dismiss = [this]() { close(); };
                auto barrier = ModalBarrier::create(
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f), true, dismiss);

                std::vector<WidgetRef> stack_children = {barrier, Positioned::fill(positioner)};
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
