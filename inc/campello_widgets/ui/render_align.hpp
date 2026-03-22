#pragma once

#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that aligns its child within itself.
     *
     * If `width_factor` or `height_factor` is set, the box sizes itself to
     * `child_size * factor` on that axis. Otherwise the box fills the available
     * space given by the incoming constraints.
     */
    class RenderAlign : public RenderBox
    {
    public:
        Alignment            alignment    = Alignment::center();
        std::optional<float> width_factor;
        std::optional<float> height_factor;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
