#include <campello_widgets/widgets/popup_menu_button.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/backdrop_filter.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_dropdown_menu_positioner.hpp>

#include <limits>

namespace systems::leal::campello_widgets
{

    namespace detail
    {
        /**
         * @brief Widget counterpart of RenderDropdownMenuPositioner, reused
         * here as-is — despite the name, the render object it wraps is
         * generic (anchor a menu-shaped child to an on-screen rect, opening
         * above or below depending on available space), not
         * DropdownButton-specific. See DropdownMenuPositionerWidget in
         * dropdown_button.hpp for the original.
         */
        class PopupMenuPositionerWidget : public SingleChildRenderObjectWidget
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

    // -------------------------------------------------------------------------
    // PopupMenuButtonState
    // -------------------------------------------------------------------------

    class PopupMenuButtonState : public State<PopupMenuButton>
    {
    public:
        void initState() override
        {
            anchor_key_ = std::make_shared<GlobalKey>();
        }

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
            gesture->key    = anchor_key_;
            gesture->on_tap = [this]() { open(); };
            gesture->child  = trigger;
            return gesture;
        }

    private:
        std::shared_ptr<OverlayEntry> menu_entry_;
        std::shared_ptr<GlobalKey>    anchor_key_;

        void open()
        {
            if (menu_entry_) return;
            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(*element());
            const Color popup_bg = w.popup_color.value_or(tokens->colors.surface);

            // Locate the trigger's own on-screen position/size so the menu
            // can open anchored to it \u2014 see
            // DropdownButton::DropdownButtonState::open()'s identical
            // lookup, which this mirrors.
            Offset anchor_pos;
            Size   anchor_size = Size::zero();
            if (anchor_key_) {
                if (auto* elem = anchor_key_->currentElement()) {
                    if (auto* roe = elem->findDescendantRenderObjectElement()) {
                        auto box = std::dynamic_pointer_cast<RenderGestureDetector>(
                            roe->sharedRenderObject());
                        if (box) {
                            anchor_pos  = box->globalOffset();
                            anchor_size = box->size();
                        }
                    }
                }
            }

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
                padded->padding = EdgeInsets::symmetric(12.0f, 0.0f);
                padded->child   = label;

                // Real M3 DropdownMenuItem rows are 48dp tall regardless of
                // the label's own text height — confirmed against a real
                // capture whose two-item menu ran a full item's worth
                // taller than ours. width_factor=1.0 shrink-wraps the row
                // to the label's natural width instead of the "fills
                // available width" default an unfactored Align/SizedBox
                // would take here — the same landmine already hit (and
                // fixed the same way) for M3 dialog action buttons in
                // MaterialDesignSystem::buildDialog().
                auto align = std::make_shared<Align>();
                align->alignment    = Alignment::centerLeft();
                align->width_factor = 1.0f;
                align->child        = padded;

                auto row = std::make_shared<ConstrainedBox>();
                row->additional_constraints =
                    BoxConstraints{0.0f, std::numeric_limits<float>::infinity(), 48.0f, 48.0f};
                row->child = align;

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
                    g->child  = row;
                    row_widget = g;
                } else {
                    auto faded     = std::make_shared<Opacity>();
                    faded->opacity = 0.40f;
                    faded->child   = row;
                    row_widget     = faded;
                }

                item_widgets.push_back(row_widget);
            }

            auto col = std::make_shared<Column>();
            col->main_axis_size       = MainAxisSize::min;
            // start (not stretch): RenderDropdownMenuPositioner measures
            // this column with a *loose* max-width equal to the live
            // viewport (see its own doc comment), and stretch always
            // claims the full incoming max rather than sizing to the
            // widest item — the same failure mode already documented and
            // fixed in DropdownButton::DropdownButtonState::open()'s
            // identical menu-column construction. Confirmed by a real
            // Android capture comparison: the menu was rendering full
            // screen width instead of shrink-wrapped to its items.
            // Known limitation: an `is_divider` item (currently unused
            // anywhere in this codebase) relies on stretch to span the
            // menu's width and will collapse to zero width without it —
            // not fixed here since nothing exercises that path today.
            col->cross_axis_alignment = CrossAxisAlignment::start;
            col->children             = std::move(item_widgets);

            WidgetRef menu_box;
            if (w.backdrop_filter.has_value()) {
                // Liquid-glass panel: the shadow needs its own DecoratedBox
                // (a flat fill there would occlude the frosted content), so
                // it wraps a BackdropFilter directly — see
                // CupertinoDesignSystem::buildCard()'s identical
                // composition for why no ClipRRect wraps the filter.
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
                menu_deco.color         = popup_bg;
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

            if (w.menu_min_width.has_value()) {
                auto constrained = std::make_shared<ConstrainedBox>();
                constrained->additional_constraints =
                    BoxConstraints{*w.menu_min_width, std::numeric_limits<float>::infinity(), 0.0f,
                                   std::numeric_limits<float>::infinity()};
                constrained->child = menu_box;
                menu_box = constrained;
            }

            // Anchor the menu to the trigger, like Flutter's real
            // PopupMenuButton — not pinned to the screen's top-right corner
            // regardless of where the button actually is. See
            // RenderDropdownMenuPositioner's doc for the above-vs-below
            // decision; it measures menu_box's actual laid-out size during
            // its own layout (it fills the Stack, so its constraints equal
            // the live viewport).
            auto positioner = std::make_shared<detail::PopupMenuPositionerWidget>();
            positioner->anchor_pos  = anchor_pos;
            positioner->anchor_size = anchor_size;
            positioner->child       = menu_box;

            // Transparent barrier for dismissal
            auto dismiss = [this]() { close(); };
            auto barrier = ModalBarrier::create(
                Color::transparent(), true, dismiss);

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

    // -------------------------------------------------------------------------

    std::unique_ptr<StateBase> PopupMenuButton::createState() const
    {
        return std::make_unique<PopupMenuButtonState>();
    }

} // namespace systems::leal::campello_widgets
