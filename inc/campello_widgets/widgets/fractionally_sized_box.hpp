#pragma once

#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>
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

        FractionallySizedBox() = default;
        explicit FractionallySizedBox(float w_factor, float h_factor, WidgetRef c = nullptr)
            : width_factor(w_factor), height_factor(h_factor)
        {
            CW_ASSERT_MSG(w_factor > 0.0f, "FractionallySizedBox.width_factor must be greater than 0");
            CW_ASSERT_MSG(h_factor > 0.0f, "FractionallySizedBox.height_factor must be greater than 0");
            child = std::move(c);
        }
        explicit FractionallySizedBox(
            float w_factor,
            float h_factor,
            Alignment align,
            WidgetRef c = nullptr)
            : width_factor(w_factor), height_factor(h_factor), alignment(align)
        {
            CW_ASSERT_MSG(w_factor > 0.0f, "FractionallySizedBox.width_factor must be greater than 0");
            CW_ASSERT_MSG(h_factor > 0.0f, "FractionallySizedBox.height_factor must be greater than 0");
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void debugValidate() const override
        {
            if (width_factor.has_value())
                CW_ASSERT_MSG(*width_factor > 0.0f, "FractionallySizedBox.width_factor must be greater than 0");
            if (height_factor.has_value())
                CW_ASSERT_MSG(*height_factor > 0.0f, "FractionallySizedBox.height_factor must be greater than 0");
        }
    };

} // namespace systems::leal::campello_widgets
