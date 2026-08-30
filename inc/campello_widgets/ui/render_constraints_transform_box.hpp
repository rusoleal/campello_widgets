#pragma once

#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes itself from its own incoming constraints (independent of
     * the child's actual size), while laying out the child with a
     * *different* set of constraints produced by `constraints_transform`.
     *
     * Shared implementation behind both `OverflowBox` and `UnconstrainedBox`
     * -- mirrors Flutter's own internal unification of the two via
     * `RenderConstraintsTransformBox`. The child may end up larger than
     * this box and paints unclipped (Flutter's default here is
     * `clipBehavior: Clip.none` too).
     */
    class RenderConstraintsTransformBox : public RenderBox
    {
    public:
        Alignment alignment = Alignment::center();
        std::function<BoxConstraints(const BoxConstraints&)> constraints_transform;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
