#pragma once

#include <campello_widgets/widgets/flex.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out children horizontally.
     *
     * A convenience subclass of `Flex` with `axis = Axis::horizontal`.
     */
    class Row : public Flex
    {
    public:
        Row() { axis = Axis::horizontal; }

        // children only — alignment defaults to start/start/max
        explicit Row(WidgetList ch)
        {
            axis     = Axis::horizontal;
            children = ch;
        }
        explicit Row(std::vector<WidgetRef> ch)
        {
            axis     = Axis::horizontal;
            children = std::move(ch);
        }

        // main + cross alignment, children last
        explicit Row(MainAxisAlignment       main,
                     CrossAxisAlignment      cross,
                     WidgetList              ch)
        {
            axis                 = Axis::horizontal;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            children             = ch;
        }
        explicit Row(MainAxisAlignment       main,
                     CrossAxisAlignment      cross,
                     std::vector<WidgetRef>  ch)
        {
            axis                 = Axis::horizontal;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            children             = std::move(ch);
        }

        // full control, children last
        explicit Row(MainAxisAlignment       main,
                     CrossAxisAlignment      cross,
                     MainAxisSize            size,
                     WidgetList              ch)
        {
            axis                 = Axis::horizontal;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            main_axis_size       = size;
            children             = ch;
        }
    };

} // namespace systems::leal::campello_widgets
