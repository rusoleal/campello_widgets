#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/stack_fit.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that positions children on top of one another.
     *
     * Non-positioned children (no left/top/right/bottom) are sized according
     * to `fit` and placed at the top-left. Positioned children are placed
     * according to their explicit position constraints.
     *
     * Children are managed via `insertChild()` / `clearChildren()`, called by
     * `StackElement::syncChildRenderObjects()`.
     */
    class RenderStack : public RenderBox
    {
    public:
        StackFit fit = StackFit::loose;

        // ------------------------------------------------------------------
        // Child management
        // ------------------------------------------------------------------

        void insertChild(
            std::shared_ptr<RenderBox> box,
            int                        index,
            std::optional<float>       left,
            std::optional<float>       top,
            std::optional<float>       right,
            std::optional<float>       bottom,
            std::optional<float>       width,
            std::optional<float>       height);

        void clearChildren();

        // ------------------------------------------------------------------
        // RenderObject overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

    private:
        struct StackChild
        {
            std::shared_ptr<RenderBox> box;
            std::optional<float>       left;
            std::optional<float>       top;
            std::optional<float>       right;
            std::optional<float>       bottom;
            std::optional<float>       width;
            std::optional<float>       height;
            Offset                     offset;
        };

        std::vector<StackChild> stack_children_;
    };

} // namespace systems::leal::campello_widgets
