#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that imposes additional constraints on its child.
     *
     * The effective constraints are the intersection of the parent's incoming
     * constraints and `additional_constraints`. Useful for enforcing minimum or
     * maximum sizes without tight-constraining the child.
     *
     * @code
     * auto w = std::make_shared<ConstrainedBox>();
     * w->additional_constraints = BoxConstraints{0, 200, 48, 48};
     * w->child = myChild;
     * @endcode
     */
    class ConstrainedBox : public SingleChildRenderObjectWidget
    {
    public:
        BoxConstraints additional_constraints;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
