#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <limits>

namespace systems::leal::campello_widgets
{

    WidgetRef ListTile::build(BuildContext&) const
    {
        // Title + optional subtitle column
        WidgetRef text_section;
        if (subtitle) {
            auto col  = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::start;
            col->children = {title, subtitle};
            text_section  = col;
        } else {
            text_section = title;
        }

        // Main row
        std::vector<WidgetRef> row_children;

        if (leading) {
            row_children.push_back(leading);
            row_children.push_back(SizedBox::create(16.0f, 0.0f));
        }

        row_children.push_back(std::make_shared<Expanded>(text_section));

        if (trailing) {
            row_children.push_back(SizedBox::create(16.0f, 0.0f));
            row_children.push_back(trailing);
        }

        auto row = std::make_shared<Row>();
        row->cross_axis_alignment = CrossAxisAlignment::center;
        row->children = std::move(row_children);

        // Content padding
        auto padded     = std::make_shared<Padding>();
        padded->padding = content_padding;
        padded->child   = row;

        // Minimum height
        const float min_h = dense ? 40.0f : (subtitle ? 72.0f : 56.0f);
        const float inf   = std::numeric_limits<float>::infinity();
        auto constrained = std::make_shared<ConstrainedBox>();
        constrained->additional_constraints = BoxConstraints{0.0f, inf, min_h, inf};
        constrained->child = padded;

        // Background color
        WidgetRef result = constrained;
        if (background_color.a > 0.0f) {
            auto colored = std::make_shared<ColoredBox>();
            colored->color = background_color;
            colored->child = constrained;
            result = colored;
        }

        // Tap / long-press
        if (on_tap || on_long_press) {
            auto gesture         = std::make_shared<GestureDetector>();
            gesture->on_tap      = on_tap;
            gesture->on_long_press = on_long_press;
            gesture->child       = result;
            result               = gesture;
        }

        // Disabled state
        if (!enabled) {
            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.38f;
            faded->child   = result;
            return faded;
        }

        return result;
    }

} // namespace systems::leal::campello_widgets
