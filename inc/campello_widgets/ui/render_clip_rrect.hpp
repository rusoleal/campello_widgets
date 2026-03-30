#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/rrect.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to a rounded rectangle.
     *
     * `border_radius` is applied to all four corners. Layout is pass-through.
     */
    class RenderClipRRect : public RenderBox
    {
    public:
        float border_radius = 0.0f;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
