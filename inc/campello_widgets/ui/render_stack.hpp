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

        /**
         * @brief Places `box` at `index`, re-applying its Positioned spec.
         *
         * If the same box is already at `index`, this reuses the slot in
         * place instead of detaching/reattaching it (see the .cpp for why
         * that distinction matters) — only its position spec is updated,
         * and only if it actually changed.
         */
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

        /**
         * @brief Drops any slots at index >= `count`, detaching their boxes.
         *
         * Used after re-inserting the first `count` children in place (see
         * `insertChild()`'s doc) to handle removals without re-parenting
         * the children that remain.
         */
        void truncateChildren(size_t count);

        // ------------------------------------------------------------------
        // RenderObject overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

        // ------------------------------------------------------------------
        // Per-child accessors -- let a subclass (RenderIndexedStack) paint/
        // hit-test only a single selected child without duplicating this
        // class's own layout logic.
        // ------------------------------------------------------------------

        size_t     childCount() const noexcept { return stack_children_.size(); }
        RenderBox* boxAt(size_t index) const noexcept
        {
            return index < stack_children_.size() ? stack_children_[index].box.get() : nullptr;
        }
        Offset offsetAt(size_t index) const noexcept
        {
            return index < stack_children_.size() ? stack_children_[index].offset : Offset{};
        }

    protected:
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
