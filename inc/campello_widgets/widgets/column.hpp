#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/flex.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out children vertically.
     *
     * A convenience subclass of `Flex` with `axis = Axis::vertical`.
     */
    class Column : public Flex
    {
    public:
        Column() { axis = Axis::vertical; }

        // children only — alignment defaults to start/start/max
        explicit Column(WidgetList ch)
        {
            axis     = Axis::vertical;
            children = ch;
        }
        explicit Column(std::vector<WidgetRef> ch)
        {
            axis     = Axis::vertical;
            children = std::move(ch);
        }

        // main + cross alignment, children last
        explicit Column(MainAxisAlignment       main,
                        CrossAxisAlignment      cross,
                        WidgetList              ch)
        {
            axis                 = Axis::vertical;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            children             = ch;
        }
        explicit Column(MainAxisAlignment       main,
                        CrossAxisAlignment      cross,
                        std::vector<WidgetRef>  ch)
        {
            axis                 = Axis::vertical;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            children             = std::move(ch);
        }

        // full control, children last
        explicit Column(MainAxisAlignment       main,
                        CrossAxisAlignment      cross,
                        MainAxisSize            size,
                        WidgetList              ch)
        {
            axis                 = Axis::vertical;
            main_axis_alignment  = main;
            cross_axis_alignment = cross;
            main_axis_size       = size;
            children             = ch;
        }

        // Factory methods
        static std::shared_ptr<Column> create(std::vector<WidgetRef> children) {
            auto c = std::make_shared<Column>();
            c->axis = Axis::vertical;
            c->children = std::move(children);
            return c;
        }

        static std::shared_ptr<Column> create(WidgetList ch) {
            auto c = std::make_shared<Column>();
            c->axis = Axis::vertical;
            c->children = ch;
            return c;
        }
    };

} // namespace systems::leal::campello_widgets
