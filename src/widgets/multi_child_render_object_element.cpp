#include <campello_widgets/widgets/multi_child_render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    MultiChildRenderObjectElement::MultiChildRenderObjectElement(
        std::shared_ptr<const MultiChildRenderObjectWidget> widget)
        : RenderObjectElement(std::move(widget))
    {
    }

    void MultiChildRenderObjectElement::update(WidgetRef new_widget)
    {
        RenderObjectElement::update(std::move(new_widget));
        rebuild();
    }

    void MultiChildRenderObjectElement::unmount()
    {
        for (auto& child : child_elements_)
            if (child) child->unmount();
        child_elements_.clear();
        RenderObjectElement::unmount();
    }

    void MultiChildRenderObjectElement::performBuild()
    {
        const auto& w = static_cast<const MultiChildRenderObjectWidget&>(*widget_);

        std::vector<std::shared_ptr<Element>> new_elements;
        new_elements.reserve(w.children.size());

        for (size_t i = 0; i < w.children.size(); ++i)
        {
            std::shared_ptr<Element> existing =
                (i < child_elements_.size()) ? child_elements_[i] : nullptr;
            new_elements.push_back(updateChild(std::move(existing), w.children[i], this));
        }

        // Unmount any children that no longer exist.
        for (size_t i = w.children.size(); i < child_elements_.size(); ++i)
            if (child_elements_[i]) updateChild(child_elements_[i], nullptr, this);

        child_elements_ = std::move(new_elements);
        syncChildRenderObjects();
    }

    void MultiChildRenderObjectElement::syncChildRenderObjects()
    {
        const auto& w = static_cast<const MultiChildRenderObjectWidget&>(*widget_);
        w.clearRenderObjectChildren(*render_object_);

        for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
        {
            if (!child_elements_[i]) continue;

            auto* child_roe =
                child_elements_[i]->findDescendantRenderObjectElement();
            if (!child_roe) continue;

            auto child_box =
                std::dynamic_pointer_cast<RenderBox>(child_roe->sharedRenderObject());
            if (child_box)
                w.insertRenderObjectChild(*render_object_, std::move(child_box), i);
        }
    }

} // namespace systems::leal::campello_widgets
