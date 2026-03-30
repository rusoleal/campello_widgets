#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_decoration.hpp>

namespace systems::leal::campello_widgets
{

    /** @brief Controls whether the decoration is painted behind or in front of the child. */
    enum class DecorationPosition
    {
        background, ///< Decoration is painted below the child (default).
        foreground, ///< Decoration is painted above the child.
    };

    /**
     * @brief Paints a BoxDecoration around (or in front of) its child.
     *
     * Layout is pass-through: the child receives the full parent constraints and
     * this box adopts the child's size. With no child, the box fills the
     * available space.
     *
     * Paint order for DecorationPosition::background:
     *   1. Box shadows
     *   2. Background fill
     *   3. Border
     *   4. Child
     *
     * For DecorationPosition::foreground the child is painted first, then
     * steps 1–3.
     */
    class RenderDecoratedBox : public RenderBox
    {
    public:
        BoxDecoration     decoration;
        DecorationPosition position = DecorationPosition::background;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        void paintDecoration(Canvas& canvas, const Offset& offset) const;
    };

} // namespace systems::leal::campello_widgets
