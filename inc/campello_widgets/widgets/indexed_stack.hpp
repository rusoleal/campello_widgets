#pragma once

#include <campello_widgets/widgets/stack.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A Stack that paints/hit-tests only the child at `index`, while
     * still sizing itself from every child and keeping every child's
     * Element/State mounted.
     *
     * Subclasses `Stack` directly (reusing its `StackElement` and
     * Positioned-child handling verbatim, since `RenderIndexedStack`
     * IS-A `RenderStack`), overriding only `createRenderObject()`/
     * `updateRenderObject()` to produce/sync a `RenderIndexedStack` instead
     * -- mirrors Flutter's own `IndexedStack extends Stack`.
     *
     * Matches Flutter's `IndexedStack` widget.
     */
    class IndexedStack : public Stack
    {
    public:
        int index = 0;

        IndexedStack() = default;
        explicit IndexedStack(int i, std::vector<WidgetRef> ch) : Stack(std::move(ch)), index(i) {}

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
