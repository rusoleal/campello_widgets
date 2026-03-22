#pragma once

#include <optional>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Aligns its child within itself using an `Alignment`.
     *
     * If `width_factor` or `height_factor` is provided the box sizes itself
     * to the corresponding child dimension multiplied by the factor.
     * Otherwise it fills the available space.
     */
    class Align : public SingleChildRenderObjectWidget
    {
    public:
        Alignment            alignment    = Alignment::center();
        std::optional<float> width_factor;
        std::optional<float> height_factor;

        Align() = default;
        explicit Align(Alignment a, WidgetRef c = nullptr)
        {
            alignment = a;
            child     = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
