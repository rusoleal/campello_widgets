#pragma once

#include <optional>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sizes its child to a fraction of the total available space.
     *
     * If `width_factor` is provided the width = parent_max_width * width_factor.
     * If `height_factor` is provided the height = parent_max_height * height_factor.
     * Omitted factors use the full available dimension.
     * `alignment` positions the child within the resulting box.
     *
     * @code
     * auto w = std::make_shared<FractionallySizedBox>();
     * w->width_factor  = 0.5f;   // half the available width
     * w->height_factor = 0.25f;  // quarter of available height
     * w->child = myChild;
     * @endcode
     */
    class FractionallySizedBox : public SingleChildRenderObjectWidget
    {
    public:
        std::optional<float> width_factor;
        std::optional<float> height_factor;
        Alignment            alignment = Alignment::center();

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
