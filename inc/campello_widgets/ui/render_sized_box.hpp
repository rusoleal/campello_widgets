#pragma once

#include <optional>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that forces its child to a specific width and/or height.
     *
     * If `width` or `height` is not set, the corresponding axis uses the maximum
     * available from the incoming constraints.
     */
    class RenderSizedBox : public RenderBox
    {
    public:
        std::optional<float> width;
        std::optional<float> height;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
