#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to its own bounding rectangle.
     *
     * Layout is pass-through: the child receives the same constraints as this
     * box and this box takes the child's size (or fills the available space if
     * no child). Paint saves the canvas, intersects the clip with
     * `Rect::fromLTWH(offset, size)`, paints the child, then restores.
     */
    class RenderClipRect : public RenderBox
    {
    public:
        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
