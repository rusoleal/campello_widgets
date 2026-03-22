#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Insets its child by the given EdgeInsets.
     */
    class Padding : public SingleChildRenderObjectWidget
    {
    public:
        EdgeInsets padding;

        Padding() = default;
        explicit Padding(EdgeInsets p, WidgetRef c = nullptr)
        {
            padding = std::move(p);
            child   = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
