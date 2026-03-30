#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Imposes additional BoxConstraints on its child.
     *
     * The effective constraints are the intersection of the parent's constraints
     * and `additional_constraints`. The child is then laid out with those tighter
     * (or equal) bounds.
     */
    class RenderConstrainedBox : public RenderBox
    {
    public:
        BoxConstraints additional_constraints;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
