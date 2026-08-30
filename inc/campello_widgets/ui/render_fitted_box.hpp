#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Scales and positions its child to fit within the available
     * space, according to `fit` and `alignment`.
     *
     * Fills the space given by its own incoming constraints (like
     * `RenderAlign` with no size factors), lays out the child unconstrained
     * to its natural/preferred size, then paints it through a canvas
     * scale+translate transform -- the same pivot-transform technique
     * `RenderTransform` uses, with the scale factor computed per `BoxFit`
     * exactly as `RenderImage::performPaint()` already does for a texture
     * (reused here against the child's laid-out size instead of texture
     * dimensions).
     *
     * Matches Flutter's `FittedBox` widget. Unlike Flutter, does not clip
     * overflow -- Flutter's own default is `clipBehavior: Clip.none` too.
     */
    class RenderFittedBox : public RenderBox
    {
    public:
        BoxFit    fit       = BoxFit::contain;
        Alignment alignment = Alignment::center();

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
