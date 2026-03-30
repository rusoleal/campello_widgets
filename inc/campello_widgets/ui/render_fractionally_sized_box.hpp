#pragma once

#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes the child to a fraction of the available space.
     *
     * If `width_factor` is set, this box's width = parent max_width * width_factor.
     * If `height_factor` is set, this box's height = parent max_height * height_factor.
     * Unset factors leave the corresponding dimension at the maximum available.
     * The child is then tight-constrained to the computed size.
     * `alignment` controls how the child is positioned within this box.
     */
    class RenderFractionallySizedBox : public RenderBox
    {
    public:
        std::optional<float> width_factor;
        std::optional<float> height_factor;
        Alignment            alignment = Alignment::center();

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
