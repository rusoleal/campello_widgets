#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that fills its bounds with a solid color, then paints
     * its child on top.
     */
    class RenderColoredBox : public RenderBox
    {
    public:
        Color color = Color::black();

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
