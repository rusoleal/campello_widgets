#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes the child to a specific aspect ratio.
     *
     * Attempts to size itself (and the child) so that `width / height == aspect_ratio`.
     * The resulting size is the largest that fits within the incoming constraints while
     * maintaining the ratio.
     */
    class RenderAspectRatio : public RenderBox
    {
    public:
        float aspect_ratio = 1.0f;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
