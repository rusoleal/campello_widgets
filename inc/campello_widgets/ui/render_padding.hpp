#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that insets its child by the given EdgeInsets.
     */
    class RenderPadding : public RenderBox
    {
    public:
        EdgeInsets padding;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
