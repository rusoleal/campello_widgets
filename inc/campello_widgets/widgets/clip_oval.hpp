#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to an oval inscribed in its bounding box.
     *
     * The clip shape is an oval that exactly fills this widget's bounds.
     * Layout is pass-through: this widget takes the same size as its child.
     */
    class ClipOval : public SingleChildRenderObjectWidget
    {
    public:
        ClipOval() = default;
        explicit ClipOval(WidgetRef c) { child = std::move(c); }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
