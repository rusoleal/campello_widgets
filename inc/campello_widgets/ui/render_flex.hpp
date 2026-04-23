#pragma once

#include <vector>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/flex_properties.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that lays out children in a single row or column.
     *
     * Implements the CSS flexbox model:
     *  - Non-flexible children (flex == 0) are laid out first to determine how
     *    much main-axis space remains.
     *  - Flexible children split the remaining space proportionally.
     *  - Children are then positioned based on `main_axis_alignment` and
     *    `cross_axis_alignment`.
     *
     * Children are managed via `insertChild()` / `clearChildren()`, which are
     * called by `FlexElement::syncChildRenderObjects()`.
     */
    class RenderFlex : public RenderBox
    {
    public:
        Axis               axis                  = Axis::horizontal;
        MainAxisAlignment  main_axis_alignment   = MainAxisAlignment::start;
        CrossAxisAlignment cross_axis_alignment  = CrossAxisAlignment::start;
        MainAxisSize       main_axis_size        = MainAxisSize::max;

        // ------------------------------------------------------------------
        // Child management
        // ------------------------------------------------------------------

        /** @brief Appends (or replaces at `index`) a child with its flex factor. */
        void insertChild(std::shared_ptr<RenderBox> box, int index, int flex);

        /** @brief Removes all children. */
        void clearChildren();

        // ------------------------------------------------------------------
        // RenderObject overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

    private:
        struct FlexChild
        {
            std::shared_ptr<RenderBox> box;
            int   flex   = 0;
            Offset offset;
        };

        std::vector<FlexChild> flex_children_;
    };

} // namespace systems::leal::campello_widgets
