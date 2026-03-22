#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Fills its bounds with a solid color, then paints its child on top.
     */
    class ColoredBox : public SingleChildRenderObjectWidget
    {
    public:
        Color color = Color::black();

        ColoredBox() = default;
        explicit ColoredBox(Color col, WidgetRef c = nullptr)
        {
            color = col;
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
