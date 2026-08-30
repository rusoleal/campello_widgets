#pragma once

#include <memory>
#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/ui/flow_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Positions/paints its children via arbitrary per-child
     * transforms supplied by `delegate`.
     *
     * Matches Flutter's `Flow` widget.
     *
     * @code
     * auto w = std::make_shared<Flow>();
     * w->delegate  = myFlowDelegate;
     * w->children  = {childA, childB, childC};
     * @endcode
     */
    class Flow : public MultiChildRenderObjectWidget
    {
    public:
        std::shared_ptr<FlowDelegate> delegate;

        Flow() = default;
        explicit Flow(std::shared_ptr<FlowDelegate> d) : delegate(std::move(d)) {}

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void insertRenderObjectChild(
            RenderObject& parent, std::shared_ptr<RenderBox> child_box, int index) const override;
        void clearRenderObjectChildren(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
