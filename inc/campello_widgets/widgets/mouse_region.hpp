#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/offset.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Detects mouse enter, exit, and hover (move) events over its child.
     *
     * Callbacks fire when the cursor enters or leaves the widget's bounds, and
     * on every hover move within the bounds. All three callbacks are optional.
     *
     * @code
     * auto region = std::make_shared<MouseRegion>();
     * region->on_enter = []()           { highlight(); };
     * region->on_exit  = []()           { unhighlight(); };
     * region->on_hover = [](Offset pos) { updateTooltip(pos); };
     * region->child    = someWidget;
     * @endcode
     */
    class MouseRegion : public SingleChildRenderObjectWidget
    {
    public:
        std::function<void()>        on_enter;
        std::function<void()>        on_exit;
        std::function<void(Offset)>  on_hover;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
