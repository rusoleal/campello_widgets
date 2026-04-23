#pragma once

#include <memory>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/scroll_physics.hpp>

namespace systems::leal::campello_widgets
{

    class ScrollController;

    /**
     * @brief A widget that makes its single child scrollable along one axis.
     *
     * The child is given unconstrained space on the scroll axis and is clipped
     * to the viewport during paint. The user can drag (pan) or use the scroll
     * wheel / trackpad to move the content.
     *
     * Usage:
     * @code
     * auto sv = std::make_shared<SingleChildScrollView>();
     * sv->child       = myLongColumn;
     * sv->scroll_axis = Axis::vertical;                        // default
     * sv->physics     = std::make_shared<BouncingScrollPhysics>(); // optional
     * @endcode
     */
    class SingleChildScrollView : public SingleChildRenderObjectWidget
    {
    public:
        Axis scroll_axis = Axis::vertical;

        /// Optional controller for programmatic scroll control.
        std::shared_ptr<ScrollController> controller;

        /// Scroll physics (defaults to ClampingScrollPhysics when null).
        std::shared_ptr<ScrollPhysics> physics;

        SingleChildScrollView() = default;
        explicit SingleChildScrollView(WidgetRef c)
        {
            child = std::move(c);
        }
        explicit SingleChildScrollView(WidgetRef c, Axis axis)
            : scroll_axis(axis)
        {
            child = std::move(c);
        }
        explicit SingleChildScrollView(
            WidgetRef c,
            Axis axis,
            std::shared_ptr<ScrollController> ctrl,
            std::shared_ptr<ScrollPhysics> phys = nullptr)
            : scroll_axis(axis)
            , controller(std::move(ctrl))
            , physics(std::move(phys))
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
