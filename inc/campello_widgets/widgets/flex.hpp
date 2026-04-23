#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/widgets/flex_element.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/flex_properties.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that lays out children in a single horizontal or vertical run.
     *
     * Equivalent to CSS flexbox. Children wrapped in `Flexible` or `Expanded`
     * receive proportional shares of the remaining main-axis space.
     *
     * Prefer `Row` and `Column` convenience subclasses.
     */
    class Flex : public MultiChildRenderObjectWidget
    {
    public:
        Axis               axis                  = Axis::horizontal;
        MainAxisAlignment  main_axis_alignment   = MainAxisAlignment::start;
        CrossAxisAlignment cross_axis_alignment  = CrossAxisAlignment::start;
        MainAxisSize       main_axis_size        = MainAxisSize::max;

        std::shared_ptr<Element> createElement() const override;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        // These are implemented but never called when FlexElement is used,
        // since FlexElement overrides syncChildRenderObjects() entirely.
        void insertRenderObjectChild(
            RenderObject&             parent,
            std::shared_ptr<RenderBox> child_box,
            int                        index) const override;

        void clearRenderObjectChildren(RenderObject& parent) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
