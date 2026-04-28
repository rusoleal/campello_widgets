#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that sizes its child to a specific aspect ratio.
     *
     * Attempts to size itself so that `width / height == aspect_ratio`, choosing
     * the largest size that fits within the parent's constraints. The child is
     * then tight-constrained to that size.
     *
     * @code
     * auto w = std::make_shared<AspectRatio>();
     * w->aspect_ratio = 16.0f / 9.0f;
     * w->child = myChild;
     * @endcode
     */
    class AspectRatio : public SingleChildRenderObjectWidget
    {
    public:
        float aspect_ratio = 1.0f;

        AspectRatio() = default;
        explicit AspectRatio(float ratio, WidgetRef c = nullptr)
            : aspect_ratio(ratio)
        {
            CW_ASSERT_MSG(ratio > 0.0f, "AspectRatio.aspect_ratio must be greater than 0");

            child = std::move(c);
        }

        
        void debugValidate() const override
        {
            CW_ASSERT_MSG(aspect_ratio > 0.0f, "AspectRatio.aspect_ratio must be greater than 0");

        }
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
