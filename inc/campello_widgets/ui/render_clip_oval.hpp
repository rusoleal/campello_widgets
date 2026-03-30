#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to an oval inscribed in its bounding rectangle.
     *
     * The clip shape is the oval that fills `Rect::fromLTWH(offset, size)`.
     * Layout is pass-through.
     */
    class RenderClipOval : public RenderBox
    {
    public:
        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
