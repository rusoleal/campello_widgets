#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to a rounded rectangle.
     *
     * `border_radius` is the radius applied to all four corners.
     * Layout is pass-through: this widget takes the same size as its child.
     *
     * @code
     * auto w = std::make_shared<ClipRRect>();
     * w->border_radius = 12.0f;
     * w->child = myChild;
     * @endcode
     */
    class ClipRRect : public SingleChildRenderObjectWidget
    {
    public:
        float border_radius = 0.0f;

        ClipRRect() = default;
        explicit ClipRRect(float radius, WidgetRef c = nullptr)
            : border_radius(radius)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
