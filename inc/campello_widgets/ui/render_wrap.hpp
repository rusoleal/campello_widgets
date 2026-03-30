#pragma once

#include <vector>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/flex_properties.hpp>
#include <campello_widgets/ui/wrap_properties.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out children in a flow that wraps onto multiple lines.
     *
     * Children are placed along the main axis until there is no room left,
     * then wrapped onto a new run. Spacing between children and between runs
     * is configurable independently.
     *
     * Children are managed via `insertChild()` / `clearChildren()`, called by
     * the element layer (MultiChildRenderObjectElement via Wrap widget hooks).
     */
    class RenderWrap : public RenderBox
    {
    public:
        Axis               direction             = Axis::horizontal;
        WrapAlignment      alignment             = WrapAlignment::start;
        float              spacing               = 0.0f;
        WrapRunAlignment   run_alignment         = WrapRunAlignment::start;
        float              run_spacing           = 0.0f;
        WrapCrossAlignment cross_axis_alignment  = WrapCrossAlignment::start;

        // ------------------------------------------------------------------
        // Child management
        // ------------------------------------------------------------------

        /** @brief Appends (or replaces at `index`) a child. */
        void insertChild(std::shared_ptr<RenderBox> box, int index);

        /** @brief Removes all children. */
        void clearChildren();

        // ------------------------------------------------------------------
        // RenderObject overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        struct WrapChild
        {
            std::shared_ptr<RenderBox> box;
            Offset                     offset;
        };

        std::vector<WrapChild> wrap_children_;
    };

} // namespace systems::leal::campello_widgets
