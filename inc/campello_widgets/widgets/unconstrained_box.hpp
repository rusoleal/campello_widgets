#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Removes all constraints from its child, letting it lay out at
     * its natural size and overflow this box's own bounds -- unclipped,
     * matching Flutter's default.
     *
     * Equivalent to `OverflowBox` with every bound left unset, but doesn't
     * carry the min/max fields.
     *
     * Matches Flutter's `UnconstrainedBox` widget.
     */
    class UnconstrainedBox : public SingleChildRenderObjectWidget
    {
    public:
        Alignment alignment = Alignment::center();

        UnconstrainedBox() = default;
        explicit UnconstrainedBox(WidgetRef c) { child = std::move(c); }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
