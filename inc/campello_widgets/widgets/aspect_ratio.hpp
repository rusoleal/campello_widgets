#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

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

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
