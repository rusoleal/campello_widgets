#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that paints its child at a given opacity.
     *
     * During the paint pass the canvas opacity is multiplied by `opacity_`
     * inside a save/restore scope. All draw calls issued by the child subtree
     * will have that effective opacity baked into their colour channels.
     *
     * Layout is transparent: the render object reports the child's size (or
     * the smallest allowed size when there is no child).
     */
    class RenderOpacity : public RenderBox
    {
    public:
        explicit RenderOpacity(float opacity = 1.0f);

        void setOpacity(float opacity) noexcept;
        float opacity() const noexcept { return opacity_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        float opacity_;
    };

} // namespace systems::leal::campello_widgets
