#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>

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
        SystemMouseCursor            cursor = SystemMouseCursor::arrow;

        MouseRegion() = default;
        explicit MouseRegion(WidgetRef c) { child = std::move(c); }
        explicit MouseRegion(WidgetRef c, SystemMouseCursor cur)
            : cursor(cur)
        {
            child = std::move(c);
        }
        explicit MouseRegion(
            WidgetRef c,
            std::function<void()> enter,
            std::function<void()> exit = nullptr)
            : on_enter(std::move(enter)), on_exit(std::move(exit))
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
