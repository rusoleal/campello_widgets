#pragma once

#include <memory>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Widget-layer bridge for RenderSliverToBoxAdapter -- wraps one
     * ordinary box widget as a sliver so it can be dropped into a
     * CustomScrollView.
     *
     * The generic SingleChildRenderObjectElement is reused unchanged (its
     * child genuinely is an ordinary box widget) -- only the parent-side
     * insertRenderObjectChild()/removeRenderObjectChild() hooks are
     * overridden, since the default SingleChildRenderObjectWidget behaviour
     * static_casts the parent to RenderBox, which RenderSliverToBoxAdapter
     * is not (it is a RenderSliver).
     */
    class SliverToBoxAdapter : public SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<RenderObject> createRenderObject() const override;

        void insertRenderObjectChild(
            RenderObject&              parent,
            std::shared_ptr<RenderBox> child_box) const override;

        void removeRenderObjectChild(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
