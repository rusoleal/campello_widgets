#pragma once

#include <functional>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/size.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to an arbitrary Path.
     *
     * `clip_path_builder` is called with this widget's size each frame to
     * produce the clip path. This lets the path adapt when the widget resizes.
     * Layout is pass-through: this widget takes the same size as its child.
     *
     * @code
     * auto w = std::make_shared<ClipPath>();
     * w->clip_path_builder = [](Size sz) {
     *     Path p;
     *     p.moveTo(sz.width * 0.5f, 0);
     *     p.lineTo(sz.width, sz.height);
     *     p.lineTo(0, sz.height);
     *     p.close();
     *     return p;
     * };
     * w->child = myChild;
     * @endcode
     */
    class ClipPath : public SingleChildRenderObjectWidget
    {
    public:
        std::function<Path(Size)> clip_path_builder;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
