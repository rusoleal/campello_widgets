#pragma once

#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/widgets/stack_element.hpp>
#include <campello_widgets/ui/stack_fit.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that stacks children on top of one another.
     *
     * Children are drawn in list order (last child on top). Wrap children in
     * `Positioned` to give them explicit position constraints within the stack.
     */
    class Stack : public MultiChildRenderObjectWidget
    {
    public:
        StackFit fit = StackFit::loose;

        Stack() = default;

        // children only
        explicit Stack(WidgetList ch)              { children = ch; }
        explicit Stack(std::vector<WidgetRef> ch)  { children = std::move(ch); }

        // fit + children last
        explicit Stack(StackFit f, WidgetList ch)
        {
            fit      = f;
            children = ch;
        }
        explicit Stack(StackFit f, std::vector<WidgetRef> ch)
        {
            fit      = f;
            children = std::move(ch);
        }

        std::shared_ptr<Element> createElement() const override;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void insertRenderObjectChild(
            RenderObject&              parent,
            std::shared_ptr<RenderBox> child_box,
            int                        index) const override;

        void clearRenderObjectChildren(RenderObject& parent) const override;

        // Factory methods
        static std::shared_ptr<Stack> create(std::vector<WidgetRef> children) {
            auto s = std::make_shared<Stack>();
            s->children = std::move(children);
            return s;
        }

        static std::shared_ptr<Stack> create(StackFit fit, std::vector<WidgetRef> children) {
            auto s = std::make_shared<Stack>();
            s->fit = fit;
            s->children = std::move(children);
            return s;
        }
    };

} // namespace systems::leal::campello_widgets
