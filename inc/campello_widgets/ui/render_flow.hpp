#pragma once

#include <vector>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/flow_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that positions/paints its children via arbitrary
     * per-child transforms supplied by a `FlowDelegate` at paint time.
     *
     * Child storage/layout/hit-test/`visitRenderChildren` structure mirrors
     * `RenderStack`'s, minus Stack's Positioned-specific per-child fields --
     * Flow has none of its own, `delegate` owns all positioning.
     *
     * Matches Flutter's `Flow` widget.
     */
    class RenderFlow : public RenderBox, public FlowPaintingContext
    {
    public:
        std::shared_ptr<FlowDelegate> delegate;

        void insertChild(std::shared_ptr<RenderBox> box, int index);
        void clearChildren();
        void truncateChildren(size_t count);

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

        // ------------------------------------------------------------------
        // FlowPaintingContext -- only valid during a paintChildren() call
        // made from within performPaint().
        // ------------------------------------------------------------------
        Size   size() const override { return size_; }
        size_t childCount() const override { return children_.size(); }
        Size   childSize(size_t index) const override;
        void   paintChild(
            size_t index,
            const systems::leal::vector_math::Matrix4<float>& transform,
            float opacity = 1.0f) override;

    private:
        std::vector<std::shared_ptr<RenderBox>> children_;

        // Set only while performPaint() is on the stack; used by paintChild().
        PaintContext* active_paint_context_ = nullptr;
        Offset        active_paint_offset_;

        // Last transform each child was painted with -- reused by
        // hitTestChildren() to map a hit point back into child-local space.
        // Sized/cleared alongside children_.
        std::vector<systems::leal::vector_math::Matrix4<float>> last_transforms_;
    };

} // namespace systems::leal::campello_widgets
