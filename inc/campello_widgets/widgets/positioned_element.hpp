#pragma once

#include <campello_widgets/widgets/stateless_element.hpp>

namespace systems::leal::campello_widgets
{

    class Positioned;

    /**
     * @brief Element for the Positioned widget.
     *
     * Positioned is otherwise a plain, transparent StatelessWidget — but its
     * left/top/right/bottom/width/height fields are read by an ancestor
     * Stack only when that Stack's own children list is reconciled
     * (StackElement::syncChildRenderObjects()). A rebuild that changes only
     * Positioned's fields — e.g. a ValueListenableBuilder several
     * StatefulWidget layers below an Overlay entry producing a new
     * Positioned each time a dragged item's pointer moves — would otherwise
     * never reach the Stack, leaving the child's on-screen offset frozen at
     * whatever it was when first mounted.
     *
     * Mirrors Flutter's ParentDataElement: every mount/update re-notifies the
     * nearest RenderObjectElement ancestor (via the existing
     * onDescendantRenderObjectChanged() bubble used elsewhere for render
     * object identity changes), so MultiChildRenderObjectElement::
     * syncChildRenderObjects() re-reads the latest Positioned and re-applies
     * the offset, regardless of how many non-RenderObject elements sit
     * between Positioned and the Stack.
     */
    class PositionedElement : public StatelessElement
    {
    public:
        using StatelessElement::StatelessElement;

    protected:
        void performBuild() override;
    };

} // namespace systems::leal::campello_widgets
