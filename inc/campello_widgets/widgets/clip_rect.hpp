#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to its own bounding rectangle.
     *
     * Any part of the child that overflows this widget's bounds is invisible.
     * Layout is pass-through: this widget takes the same size as its child.
     */
    class ClipRect : public SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
