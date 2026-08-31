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
        if (index < 0) index = 0;
        if (index >= static_cast<int>(stack_children_.size()))
            stack_children_.resize(index + 1);

        auto& sc = stack_children_[index];

        // Same box already occupying this slot — e.g. StackElement
        // re-syncing every sibling because just ONE of them (a Positioned
        // fed by a ValueListenableBuilder, such as a drag feedback overlay
        // tracking the cursor) got new position fields, while the others
        // — including, at the root Overlay's Stack, the entire rest of the
        // app — are unchanged. Detaching and reattaching those via
        // setParent() would be needless churn (repeated markNeedsLayout()
        // bubbling, and any attach()/detach() side effects on the box
        // itself, e.g. RenderGestureDetector re-registering with the
        // PointerDispatcher) for content that hasn't actually moved.
        if (sc.box == box)
        {
            if (sc.left != left || sc.top != top || sc.right != right ||
                sc.bottom != bottom || sc.width != width || sc.height != height)
            {
                sc.left = left; sc.top = top; sc.right = right;
                sc.bottom = bottom; sc.width = width; sc.height = height;
                markNeedsLayout();
            }
            return;
        }

        if (sc.box) sc.box->setParent(nullptr);
        sc = {std::move(box), left, top, right, bottom, width, height, {}};
        if (sc.box) sc.box->setParent(this);
        markNeedsLayout();
    }

    void RenderStack::clearChildren()
    {
        for (auto& sc : stack_children_)
            if (sc.box) sc.box->setParent(nullptr);
        stack_children_.clear();
        markNeedsLayout();
    }

    void RenderStack::truncateChildren(size_t count)
    {
        if (count >= stack_children_.size()) return;
        for (size_t i = count; i < stack_children_.size(); ++i)
            if (stack_children_[i].box) stack_children_[i].box->setParent(nullptr);
        stack_children_.resize(count);
        markNeedsLayout();
    }

    void RenderStack::performLayout()
    {
        // Pass 1: non-positioned children first. StackFit::expand claims all
        // available space up front (matches the old unconditional
        // behavior), but StackFit::loose/passthrough (the default) must
        // size the stack to CONTAIN its children, not just grab the full
        // incoming max constraints — those can be effectively unbounded
        // (e.g. the vertical axis inside a SingleChildScrollView), which
        // would make the stack balloon to a huge size for one layout pass
        // instead of matching its content.
        float content_w = 0.0f;
        float content_h = 0.0f;

        const Size expand_size =
            constraints_.constrain({constraints_.max_width, constraints_.max_height});

        for (auto& sc : stack_children_)
        {
            if (!sc.box) continue;

            const bool is_positioned =
                sc.left || sc.top || sc.right || sc.bottom || sc.width || sc.height;
            if (is_positioned) continue;

            // Non-positioned: size according to StackFit.
            BoxConstraints cc;
            switch (fit)
            {
                case StackFit::loose:
                    cc = constraints_.loosen(); break;
                case StackFit::expand:
                    cc = BoxConstraints::tight(expand_size); break;
                case StackFit::passthrough:
                    cc = constraints_; break;
            }
            sc.box->layout(cc);
            sc.offset = {0.0f, 0.0f};
            content_w = std::max(content_w, sc.box->size().width);
            content_h = std::max(content_h, sc.box->size().height);
        }

        size_ = (fit == StackFit::expand)
            ? expand_size
            : constraints_.constrain({content_w, content_h});

        // Pass 2: positioned children, now that size_ (needed for their
        // percentage/edge math below) is final.
        for (auto& sc : stack_children_)
        {
            if (!sc.box) continue;

            const bool is_positioned =
                sc.left || sc.top || sc.right || sc.bottom || sc.width || sc.height;

            if (is_positioned)
            {
                // Positioned: derive constraints from position specs.
                // Only edges that pin both sides (or an explicit width/height)
                // determine a definite size; an edge pinned on one side only
                // (e.g. just `left`/`top`, as a feedback overlay following a
                // cursor) leaves the child to size itself intrinsically, so
                // it must get loose — not tight — constraints up to the
                // remaining space.
                float x = sc.left.value_or(0.0f);
                float y = sc.top.value_or(0.0f);

                const bool width_determined  = sc.width.has_value() || (sc.left && sc.right);
                const bool height_determined = sc.height.has_value() || (sc.top && sc.bottom);

                float child_w = sc.width.value_or(
                    sc.left && sc.right
                        ? std::max(0.0f, size_.width - *sc.left - *sc.right)
                        : constraints_.max_width);

                float child_h = sc.height.value_or(
                    sc.top && sc.bottom
                        ? std::max(0.0f, size_.height - *sc.top - *sc.bottom)
                        : constraints_.max_height);

                child_w = std::clamp(child_w, 0.0f, size_.width);
                child_h = std::clamp(child_h, 0.0f, size_.height);

                sc.box->layout(BoxConstraints{
                    width_determined  ? child_w : 0.0f, child_w,
                    height_determined ? child_h : 0.0f, child_h,
                });

                // Edges pinned on only one side (right without left, bottom
                // without top — e.g. "open upward, sized to content") must
                // be positioned using the child's ACTUAL laid-out size, not
                // child_w/child_h above — those are just the incoming
                // constraint bound (e.g. the full stack size) whenever the
                // child was left to size itself intrinsically, not what it
                // actually chose to be.
                const Size actual = sc.box->size();
                if (!sc.left && sc.right)
                    x = size_.width - *sc.right - actual.width;
                if (!sc.top && sc.bottom)
                    y = size_.height - *sc.bottom - actual.height;

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
        bool hit_any = false;
        for (auto it = stack_children_.rbegin(); it != stack_children_.rend(); ++it)
        {
            if (!it->box) continue;
            if (it->box->hitTest(result, position - it->offset))
            {
                hit_any = true;
                // A non-opaque claim (HitTestBehavior::translucent, see
                // hit_test.hpp) doesn't block further, lower-painted
                // siblings from also being hit-tested at this point --
                // only an opaque hit stops the walk. The entry this child
                // just appended is always the last one in the path (its
                // own hitTest() adds it after recursing into its own
                // descendants).
                if (result.path().back().opaque)
                    return true;
                continue;
            }
        }
        return hit_any;
    }

    void RenderStack::visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const
    {
        for (const auto& child : stack_children_)
            if (child.box) visitor(child.box.get());
    }

} // namespace systems::leal::campello_widgets
