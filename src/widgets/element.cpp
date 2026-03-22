#include <campello_widgets/widgets/element.hpp>

namespace systems::leal::campello_widgets
{

    Element::Element(WidgetRef widget)
        : widget_(std::move(widget))
    {
    }

    const Widget& Element::widget() const
    {
        return *widget_;
    }

    void Element::mount(Element* parent)
    {
        parent_ = parent;
        dirty_  = true;
        rebuild();
    }

    void Element::unmount()
    {
        parent_ = nullptr;
    }

    void Element::update(WidgetRef new_widget)
    {
        widget_ = std::move(new_widget);
        markNeedsBuild();
    }

    void Element::markNeedsBuild()
    {
        dirty_ = true;
        // TODO: notify the framework scheduler for deferred rebuild
    }

    void Element::rebuild()
    {
        if (!dirty_) return;
        dirty_ = false;
        performBuild();
    }

    RenderObjectElement* Element::findDescendantRenderObjectElement() noexcept
    {
        if (auto* roe = nearestRenderObjectElement()) return roe;
        Element* child = firstChildElement();
        return child ? child->findDescendantRenderObjectElement() : nullptr;
    }

    std::shared_ptr<Element> Element::updateChild(
        std::shared_ptr<Element> child,
        WidgetRef                new_widget,
        Element*                 parent)
    {
        if (!new_widget)
        {
            if (child) child->unmount();
            return nullptr;
        }

        if (child)
        {
            if (child->widget().widgetType() == new_widget->widgetType())
            {
                child->update(std::move(new_widget));
                return child;
            }
            child->unmount();
        }

        auto new_element = new_widget->createElement();
        new_element->mount(parent);
        return new_element;
    }

} // namespace systems::leal::campello_widgets
