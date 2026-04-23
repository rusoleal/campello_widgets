#include <algorithm>
#include <campello_widgets/ui/render_stack.hpp>

namespace systems::leal::campello_widgets
{

    void RenderStack::insertChild(
        std::shared_ptr<RenderBox> box,
        int                        index,
        std::optional<float>       left,
        std::optional<float>       top,
        std::optional<float>       right,
        std::optional<float>       bottom,
        std::optional<float>       width,
        std::optional<float>       height)
    {
        if (index >= static_cast<int>(stack_children_.size()))
            stack_children_.resize(index + 1);
        if (stack_children_[index].box) stack_children_[index].box->setParent(nullptr);
        stack_children_[index] = {std::move(box), left, top, right, bottom, width, height, {}};
        if (stack_children_[index].box) stack_children_[index].box->setParent(this);
        markNeedsLayout();
    }

    void RenderStack::clearChildren()
    {
        for (auto& sc : stack_children_)
            if (sc.box) sc.box->setParent(nullptr);
        stack_children_.clear();
        markNeedsLayout();
    }

    void RenderStack::performLayout()
    {
        // The stack fills all available space.
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});

        for (auto& sc : stack_children_)
        {
            if (!sc.box) continue;

            const bool is_positioned =
                sc.left || sc.top || sc.right || sc.bottom || sc.width || sc.height;

            if (!is_positioned)
            {
                // Non-positioned: size according to StackFit.
                BoxConstraints cc;
                switch (fit)
                {
                    case StackFit::loose:
                        cc = constraints_.loosen(); break;
                    case StackFit::expand:
                        cc = BoxConstraints::tight(size_); break;
                    case StackFit::passthrough:
                        cc = constraints_; break;
                }
                sc.box->layout(cc);
                sc.offset = {0.0f, 0.0f};
            }
            else
            {
                // Positioned: derive constraints from position specs.
                float x = sc.left.value_or(0.0f);
                float y = sc.top.value_or(0.0f);

                float child_w = sc.width.value_or(
                    sc.left && sc.right
                        ? std::max(0.0f, size_.width - *sc.left - *sc.right)
                        : constraints_.max_width);

                float child_h = sc.height.value_or(
                    sc.top && sc.bottom
                        ? std::max(0.0f, size_.height - *sc.top - *sc.bottom)
                        : constraints_.max_height);

                if (!sc.left && sc.right)
                    x = size_.width - *sc.right - child_w;

                if (!sc.top && sc.bottom)
                    y = size_.height - *sc.bottom - child_h;

                sc.box->layout(BoxConstraints::tight(
                    std::clamp(child_w, 0.0f, size_.width),
                    std::clamp(child_h, 0.0f, size_.height)));
                sc.offset = {x, y};
            }
        }
    }

    void RenderStack::performPaint(PaintContext& context, const Offset& origin)
    {
        for (const auto& sc : stack_children_)
        {
            if (sc.box)
                sc.box->paint(context, {origin.x + sc.offset.x, origin.y + sc.offset.y});
        }
    }

    bool RenderStack::hitTestChildren(HitTestResult& result, const Offset& position)
    {
        // Walk back-to-front: last child is painted on top and gets first hit.
        for (auto it = stack_children_.rbegin(); it != stack_children_.rend(); ++it)
        {
            if (!it->box) continue;
            if (it->box->hitTest(result, position - it->offset))
                return true;
        }
        return false;
    }

    void RenderStack::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& child : stack_children_)
            if (child.box) visitor(child.box.get());
    }

} // namespace systems::leal::campello_widgets
