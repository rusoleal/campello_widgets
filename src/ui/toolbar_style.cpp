#include <campello_widgets/ui/toolbar_style.hpp>

#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
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
        // One item: a pill (filled when selected, an overlay while
        // hovered) holding its icon and/or label, with its tooltip.
        class ToolbarItemWidget : public StatefulWidget
        {
        public:
            ToolbarConfig::Item item;
            ToolbarStyle style;
            bool pointer_cursor = true;

            std::unique_ptr<StateBase> createState() const override;
        };

        class ToolbarItemState : public State<ToolbarItemWidget>
        {
        public:
            WidgetRef build(BuildContext&) override
            {
                const auto& w  = widget();
                const auto& s  = w.style;
                const auto& it = w.item;
                const bool active = it.enabled && static_cast<bool>(it.on_pressed);
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

                // Icon only: a square item (the touch target, the icon
                // centred: (extent - icon) / 2 all round); with a label:
                // padded along.
                auto box    = std::make_shared<SizedBox>();
                box->height = s.item_extent;
                if (it.label.empty()) {
                    box->width = s.item_extent;
                    box->child = std::make_shared<Center>(row);
                } else {
                    // No Center here: it would take all the width offered
                    // (a whole Row's); the row centres itself vertically in
                    // the box's fixed height.
                    auto padded     = std::make_shared<Padding>();
                    padded->padding = EdgeInsets::symmetric(0.0f, s.item_padding);
                    padded->child   = row;
                    box->child      = padded;
                }

                BoxDecoration overlay;
                overlay.color         = hovered_ && active ? s.hover_overlay : Color::transparent();
                overlay.border_radius = s.item_radius;
                auto hover        = std::make_shared<DecoratedBox>();
                hover->decoration = overlay;
                hover->child      = box;

                BoxDecoration fill;
                fill.color         = it.selected ? s.selected_container : Color::transparent();
                fill.border_radius = s.item_radius;
                auto pill        = std::make_shared<DecoratedBox>();
                pill->decoration = fill;
                pill->child      = hover;

                auto gesture    = std::make_shared<GestureDetector>();
                gesture->on_tap = active ? it.on_pressed : nullptr;
                gesture->child  = pill;

                auto region      = std::make_shared<MouseRegion>();
                region->cursor   = active && w.pointer_cursor ? SystemMouseCursor::pointer : SystemMouseCursor::arrow;
                region->on_enter = [this]() { setState([this] { hovered_ = true; }); };
                region->on_exit  = [this]() { setState([this] { hovered_ = false; }); };
                region->child    = gesture;

                WidgetRef result = region;
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
            bool hovered_ = false;
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
        if (config.floating) deco.box_shadow = style.shadow;
        auto container        = std::make_shared<DecoratedBox>();
        container->decoration = deco;
        container->child      = padded;
        return container;
    }

} // namespace systems::leal::campello_widgets
