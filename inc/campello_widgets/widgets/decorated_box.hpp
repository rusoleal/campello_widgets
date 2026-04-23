#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/render_decorated_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that paints a BoxDecoration around its child.
     *
     * @code
     * DecoratedBox box;
     * box.decoration = BoxDecoration{
     *     .color         = Color::fromRGB(0.2f, 0.5f, 0.9f),
     *     .border_radius = 8.0f,
     *     .border        = BoxBorder::all(Color::black(), 1.5f),
     * };
     * box.child = someChild;
     * @endcode
     */
    class DecoratedBox : public SingleChildRenderObjectWidget
    {
    public:
        BoxDecoration      decoration;
        DecorationPosition position = DecorationPosition::background;

        DecoratedBox() = default;
        explicit DecoratedBox(BoxDecoration dec, WidgetRef c = nullptr)
            : decoration(std::move(dec))
        {
            child = std::move(c);
        }
        explicit DecoratedBox(BoxDecoration dec, DecorationPosition pos, WidgetRef c = nullptr)
            : decoration(std::move(dec)), position(pos)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
