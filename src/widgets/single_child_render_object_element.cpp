#include <campello_widgets/widgets/single_child_render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{

    SingleChildRenderObjectElement::SingleChildRenderObjectElement(
        std::shared_ptr<const SingleChildRenderObjectWidget> widget)
        : RenderObjectElement(std::move(widget))
    {
    }

    void SingleChildRenderObjectElement::update(WidgetRef new_widget)
    {
        RenderObjectElement::update(std::move(new_widget));
        rebuild();
    }

    void SingleChildRenderObjectElement::unmount()
    {
        if (child_element_)
        {
            child_element_->unmount();
            child_element_.reset();
        }
        RenderObjectElement::unmount();
    }

    void SingleChildRenderObjectElement::performBuild()
    {
        const auto& w = static_cast<const SingleChildRenderObjectWidget&>(*widget_);
        child_element_ = updateChild(child_element_, w.child, this);
        syncChildRenderObject();
    }

    void SingleChildRenderObjectElement::onDescendantRenderObjectChanged()
    {
        // Re-wire the child render object; don't propagate further — we handle it here.
        syncChildRenderObject();
    }

    void SingleChildRenderObjectElement::syncChildRenderObject()
    {
        const auto& w = static_cast<const SingleChildRenderObjectWidget&>(*widget_);

        std::cerr << "[SingleChildRenderObjectElement] syncChildRenderObject: has_child=" 
                  << (child_element_ ? "yes" : "no") << "\n";

        if (!child_element_)
        {
            w.removeRenderObjectChild(*render_object_);
            return;
        }

        auto* child_roe = child_element_->findDescendantRenderObjectElement();
        std::cerr << "[SingleChildRenderObjectElement] child_roe=" << child_roe << "\n";
        if (child_roe)
        {
            auto child_box = std::dynamic_pointer_cast<RenderBox>(
                child_roe->sharedRenderObject());
            std::cerr << "[SingleChildRenderObjectElement] child_box=" << child_box.get() << "\n";
            if (child_box)
                w.insertRenderObjectChild(*render_object_, std::move(child_box));
        }
    }

} // namespace systems::leal::campello_widgets
