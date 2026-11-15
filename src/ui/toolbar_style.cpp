#include <campello_widgets/ui/toolbar_style.hpp>

#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/icon.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/tooltip.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // The toolbar's items' focus nodes, in order, for arrow-key moves
        // between them (rebuilt with the toolbar; each item's state keeps
        // its own node, so focus survives rebuilds).
        struct ToolbarFocusGroup
        {
            std::vector<std::weak_ptr<FocusNode>> nodes;
            std::vector<bool> focusable;
            Axis direction = Axis::horizontal;

            // Focus the next/previous focusable item from `from`, wrapping;
            // Home/End: from -1 forwards, from size backwards.
            bool move(int from, int step)
            {
                const int n = static_cast<int>(nodes.size());
                for (int k = 1; k <= n; ++k) {
                    const int i = (((from + step * k) % n) + n) % n;
                    if (focusable[static_cast<std::size_t>(i)]) {
                        if (auto node = nodes[static_cast<std::size_t>(i)].lock()) {
                            node->requestFocus();
                            return true;
                        }
                    }
                }
                return false;
            }
        };

        // One item: a pill (filled when selected, an overlay while
        // hovered) holding its icon and/or label, with its tooltip.
        class ToolbarItemWidget : public StatefulWidget
        {
        public:
            ToolbarConfig::Item item;
            ToolbarStyle style;
            bool pointer_cursor = true;
            std::shared_ptr<ToolbarFocusGroup> group;
            int index = 0;
            bool tab_stop = false;  ///< the toolbar's one Tab stop

            std::unique_ptr<StateBase> createState() const override;
        };

        class ToolbarItemState : public State<ToolbarItemWidget>
        {
        public:
            void initState() override
            {
                node_ = std::make_shared<FocusNode>();
                // Arrow keys bubble up from the item's own node (its
                // GestureDetector handles Space/Enter there) to this one,
                // which can't itself take focus.
                keys_ = std::make_shared<FocusNode>();
                keys_->setCanRequestFocus(false);
                keys_->setSkipTraversal(true);
            }

            WidgetRef build(BuildContext&) override
            {
                const auto& w  = widget();
                const auto& s  = w.style;
                const auto& it = w.item;
                const bool active = it.enabled && static_cast<bool>(it.on_pressed);

                // Roving focus: one Tab stop for the toolbar, arrows within.
                node_->setSkipTraversal(!(w.tab_stop && active));
                if (w.group && w.index < static_cast<int>(w.group->nodes.size())) {
                    w.group->nodes[static_cast<std::size_t>(w.index)] = node_;
                }
                keys_->on_key = [group = w.group, index = w.index](const KeyEvent& event) {
                    if (!group || event.kind == KeyEventKind::up) return false;
                    const bool horizontal = group->direction == Axis::horizontal;
                    const int n = static_cast<int>(group->nodes.size());
                    if (event.key_code == (horizontal ? KeyCode::right : KeyCode::down)) return group->move(index, 1);
                    if (event.key_code == (horizontal ? KeyCode::left : KeyCode::up)) return group->move(index, -1);
                    if (event.key_code == KeyCode::home) return group->move(-1, 1);
                    if (event.key_code == KeyCode::end) return group->move(n, -1);
                    return false;
                };
                const bool ring = focused_ && active && FocusManager::highlightMode() == FocusHighlightMode::keyboard;
                const Color fg = it.selected ? s.selected_foreground : s.foreground;

                std::vector<WidgetRef> content;
                if (it.icon_builder) {
                    content.push_back(it.icon_builder(fg));
                } else if (it.icon) {
                    WidgetRef icon = it.icon;
                    if (auto asIcon = std::dynamic_pointer_cast<const Icon>(it.icon)) {
                        icon = Icon::create(asIcon->texture, s.icon_size, fg);
                    }
                    content.push_back(icon);
                }
                if (!it.label.empty()) {
                    if (!content.empty()) content.push_back(std::make_shared<SizedBox>(8.0f, 0.0f));
                    TextStyle label = s.label_style;
                    label.withColor(fg);
                    content.push_back(std::make_shared<Text>(it.label, label));
                }
                auto row = std::make_shared<Row>();
                row->main_axis_size       = MainAxisSize::min;
                row->main_axis_alignment  = MainAxisAlignment::center;
                row->cross_axis_alignment = CrossAxisAlignment::center;
                row->children             = std::move(content);

                // The indicator: the selected fill and the state layers
                // (hover, pressed) in the content colour, `indicator_extent`
                // tall, centred in the `item_extent` touch target. Icon
                // only: a square (a circle with full corners); with a
                // label: padded along it.
                const float stateOpacity = !active ? 0.0f
                                         : pressed_ ? s.pressed_opacity
                                         : std::max(hovered_ ? s.hover_opacity : 0.0f, ring ? s.focus_opacity : 0.0f);
                WidgetRef inner;
                if (it.label.empty()) {
                    inner = std::make_shared<SizedBox>(s.indicator_extent, s.indicator_extent, std::make_shared<Center>(row));
                } else {
                    // No Center around the row: it would take all the
                    // width offered (a whole Row's).
                    auto padded     = std::make_shared<Padding>();
                    padded->padding = EdgeInsets::symmetric(0.0f, s.item_padding);
                    padded->child   = row;
                    inner = std::make_shared<SizedBox>(std::nullopt, s.indicator_extent, padded);
                }
                BoxDecoration overlay;
                overlay.color         = Color::fromRGBA(fg.r, fg.g, fg.b, fg.a * stateOpacity);
                overlay.border_radius = s.item_radius;
                auto layer        = std::make_shared<DecoratedBox>();
                layer->decoration = overlay;
                layer->child      = inner;

                BoxDecoration fill;
                fill.color         = it.selected ? s.selected_container : Color::transparent();
                fill.border_radius = s.item_radius;
                auto pill        = std::make_shared<DecoratedBox>();
                pill->decoration = fill;
                pill->child      = layer;

                // The focus ring: `focus_ring_width` wide, `focus_ring_offset`
                // outside the indicator, its shape (may reach past the
                // touch target: the Stack doesn't clip).
                WidgetRef indicator = pill;
                if (ring && s.focus_ring.a > 0.0f) {
                    const float out = s.focus_ring_offset + s.focus_ring_width;
                    BoxDecoration border;
                    border.border        = BoxBorder::all(s.focus_ring, s.focus_ring_width);
                    border.border_radius = s.item_radius;
                    auto outline        = std::make_shared<DecoratedBox>();
                    outline->decoration = border;
                    indicator = Stack::create({pill, std::make_shared<Positioned>(-out, -out, -out, -out, std::nullopt, std::nullopt, outline)});
                }

                // The touch target round it.
                auto target    = std::make_shared<SizedBox>();
                target->height = s.item_extent;
                if (it.label.empty()) target->width = s.item_extent;
                target->child  = std::make_shared<Center>(indicator);

                auto gesture    = std::make_shared<GestureDetector>();
                gesture->on_tap = active ? it.on_pressed : nullptr;
                if (active) {
                    gesture->on_tap_down   = [this](TapDownDetails) { setState([this] { pressed_ = true; }); };
                    gesture->on_tap_up     = [this](TapUpDetails) { setState([this] { pressed_ = false; }); };
                    gesture->on_tap_cancel = [this]() { setState([this] { pressed_ = false; }); };
                }
                gesture->child  = target;
                gesture->focusable       = active;
                gesture->focus_node      = node_;
                gesture->on_focus_change = [this](bool has_focus) { setState([this, has_focus] { focused_ = has_focus; }); };

                auto region      = std::make_shared<MouseRegion>();
                region->cursor   = active && w.pointer_cursor ? SystemMouseCursor::pointer : SystemMouseCursor::arrow;
                region->on_enter = [this]() { setState([this] { hovered_ = true; }); };
                region->on_exit  = [this]() { setState([this] { hovered_ = false; pressed_ = false; }); };
                region->child    = gesture;

                auto keys        = std::make_shared<Focus>();
                keys->focus_node = keys_;
                keys->child      = region;

                WidgetRef result = keys;
                if (!it.tooltip.empty()) {
                    result = std::make_shared<Tooltip>(it.tooltip, result);
                }
                if (!it.enabled) {
                    auto faded     = std::make_shared<Opacity>();
                    faded->opacity = s.disabled_opacity;
                    faded->child   = result;
                    result = faded;
                }
                return result;
            }

        private:
            std::shared_ptr<FocusNode> node_;
            std::shared_ptr<FocusNode> keys_;
            bool hovered_ = false;
            bool pressed_ = false;
            bool focused_ = false;
        };

        std::unique_ptr<StateBase> ToolbarItemWidget::createState() const
        {
            return std::make_unique<ToolbarItemState>();
        }
    }

    WidgetRef buildToolbarWithStyle(const ToolbarConfig& config, const ToolbarStyle& style, bool pointer_cursor)
    {
        const bool horizontal = config.direction == Axis::horizontal;
        // Fixed both ways (from_width() would stretch across the toolbar's
        // loose constraints, a viewer's whole height when floating).
        const auto gap = [&](float extent) -> WidgetRef {
            return horizontal ? std::make_shared<SizedBox>(extent, 0.0f) : std::make_shared<SizedBox>(0.0f, extent);
        };
        // Keyboard: the items' focus nodes in order; Tab enters at the
        // selected item, else the first enabled one.
        auto focus = std::make_shared<ToolbarFocusGroup>();
        focus->direction = config.direction;
        int tabStop = -1;
        for (const auto& group : config.groups) {
            for (const auto& it : group) {
                const bool active = it.enabled && static_cast<bool>(it.on_pressed);
                if (active && it.selected && tabStop < 0) tabStop = static_cast<int>(focus->focusable.size());
                focus->focusable.push_back(active);
            }
        }
        if (tabStop < 0) {
            for (std::size_t i = 0; i < focus->focusable.size(); ++i) {
                if (focus->focusable[i]) { tabStop = static_cast<int>(i); break; }
            }
        }
        focus->nodes.resize(focus->focusable.size());
        int index = 0;

        std::vector<WidgetRef> children;
        for (const auto& group : config.groups) {
            if (group.empty()) continue;
            if (!children.empty()) {
                // Between groups: a gap, and a short divider where drawn.
                children.push_back(gap(style.group_gap));
                if (style.divider.a > 0.0f) {
                    auto line = std::make_shared<SizedBox>();
                    line->width  = horizontal ? 1.0f : style.item_extent * 0.5f;
                    line->height = horizontal ? style.item_extent * 0.5f : 1.0f;
                    line->child  = std::make_shared<ColoredBox>(style.divider);
                    children.push_back(line);
                    children.push_back(gap(style.group_gap));
                }
            }
            for (std::size_t i = 0; i < group.size(); ++i) {
                if (i > 0) children.push_back(gap(style.gap));
                auto item            = std::make_shared<ToolbarItemWidget>();
                item->item           = group[i];
                item->style          = style;
                item->pointer_cursor = pointer_cursor;
                item->group          = focus;
                item->index          = index;
                item->tab_stop       = index == tabStop;
                ++index;
                children.push_back(item);
            }
        }

        WidgetRef line;
        if (horizontal) {
            auto row = std::make_shared<Row>();
            row->main_axis_size       = MainAxisSize::min;
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->children             = std::move(children);
            line = row;
        } else {
            auto column = std::make_shared<Column>();
            column->main_axis_size       = MainAxisSize::min;
            column->cross_axis_alignment = CrossAxisAlignment::center;
            column->children             = std::move(children);
            line = column;
        }

        auto padded     = std::make_shared<Padding>();
        padded->padding = style.padding;
        padded->child   = line;

        BoxDecoration deco;
        deco.color         = config.floating ? style.container : style.docked_container;
        deco.border_radius = config.floating ? style.radius : 0.0f;
        if (config.floating && config.elevated) deco.box_shadow = style.shadow;
        auto container        = std::make_shared<DecoratedBox>();
        container->decoration = deco;
        container->child      = padded;
        return container;
    }

} // namespace systems::leal::campello_widgets
