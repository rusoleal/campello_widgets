#pragma once

#include <campello_widgets/ui/render_stack.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderStack that paints/hit-tests only its child at `index`.
     *
     * Layout is unchanged from `RenderStack` -- this box still sizes itself
     * from *all* children (matching Flutter's `IndexedStack`, whose whole
     * point is that switching the visible index doesn't reflow the box),
     * only paint and hit-testing are restricted to the one selected child.
     *
     * Matches Flutter's `IndexedStack` widget's render object.
     */
    class RenderIndexedStack : public RenderStack
    {
    public:
        /** @brief Which child to paint/hit-test. -1 = none. */
        int index = 0;

        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
    };

} // namespace systems::leal::campello_widgets
