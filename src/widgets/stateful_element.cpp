#include <campello_widgets/widgets/stateful_element.hpp>

namespace systems::leal::campello_widgets
{

    StatefulElement::StatefulElement(std::shared_ptr<const StatefulWidget> widget)
        : Element(widget) // keep a shared_ptr alive before createState()
        , state_(static_cast<const StatefulWidget&>(*widget).createState())
    {
        state_->element_        = this;
        state_->current_widget_ = widget_.get();
    }

    StatefulElement::~StatefulElement()
    {
        if (state_)
        {
            state_->element_        = nullptr;
            state_->current_widget_ = nullptr;
        }
    }

    void StatefulElement::mount(Element* parent)
    {
        state_->initState();
        Element::mount(parent);
    }

    void StatefulElement::unmount()
    {
        if (child_)
            child_->unmount();
        state_->dispose();
        state_->element_        = nullptr;
        state_->current_widget_ = nullptr;
        Element::unmount();
    }

    void StatefulElement::update(WidgetRef new_widget)
    {
        // Keep the old widget alive (as a shared_ptr, not just a reference)
        // across Element::update() below. That call reassigns widget_,
        // which — for the common case where nothing else references the
        // previous widget (a fresh WidgetRef built fresh each parent
        // rebuild, the normal pattern) — releases the last owning
        // shared_ptr and destroys it immediately. A plain `const Widget&`
        // bound to *widget_ would dangle the moment that happens, making
        // every didUpdateWidget() override that reads the old widget's
        // fields (AnimatedSwitcher's child, AnimatedAlign's alignment,
        // AnimatedBuilder's animation controller, ...) undefined behavior.
        WidgetRef old_widget = widget_;
        Element::update(std::move(new_widget));
        state_->current_widget_ = widget_.get();
        state_->didUpdateWidget(*old_widget);
    }

    void StatefulElement::scheduleBuild()
    {
        markNeedsBuild();
    }

    void StatefulElement::performBuild()
    {
        WidgetRef built     = state_->build(*this);
        auto      old_child = child_;
        child_              = updateChild(child_, std::move(built), this);

        // See StatelessElement::performBuild() for why this notification is
        // needed: setState() causing the built subtree's widget type to
        // change (e.g. loading indicator -> image) can swap in a brand new
        // child element whose render object differs from what an ancestor
        // RenderObjectElement last synced — that ancestor never rebuilds on
        // its own just because this deferred child did, so it must be told
        // explicitly.
        if (child_ != old_child)
            onDescendantRenderObjectChanged();
    }

} // namespace systems::leal::campello_widgets
