#pragma once

#include <functional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/path.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to an arbitrary Path.
     *
     * `clip_path_builder` is called during paint with the current box size to
     * produce the clip path. This allows the path to scale with the widget.
     * Layout is pass-through.
     *
     * @code
     * auto ro = std::make_shared<RenderClipPath>();
     * ro->clip_path_builder = [](Size sz) {
     *     Path p;
     *     p.moveTo(sz.width * 0.5f, 0);
     *     p.lineTo(sz.width, sz.height);
     *     p.lineTo(0, sz.height);
     *     p.close();
     *     return p;
     * };
     * @endcode
     */
    class RenderClipPath : public RenderBox
    {
    public:
        std::function<Path(Size)> clip_path_builder;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
