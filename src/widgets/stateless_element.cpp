#include <campello_widgets/widgets/stateless_element.hpp>

namespace systems::leal::campello_widgets
{

    StatelessElement::StatelessElement(std::shared_ptr<const StatelessWidget> widget)
        : Element(std::move(widget))
    {
    }

    void StatelessElement::unmount()
    {
        if (child_)
            child_->unmount();
        Element::unmount();
    }

    void StatelessElement::update(WidgetRef new_widget)
    {
        Element::update(std::move(new_widget));
    }

    void StatelessElement::performBuild()
    {
        const auto& w        = static_cast<const StatelessWidget&>(*widget_);
        WidgetRef   built    = w.build(*this);
        auto        old_child = child_;
        child_               = updateChild(child_, std::move(built), this);

        // If reconciliation swapped in a different child element (e.g. the
        // built widget's type changed), the nearest ancestor
        // RenderObjectElement's cached child render object is now stale —
        // notify it to re-sync. Without this, ancestors whose own rebuild
        // already ran this frame (via the deferred markNeedsBuild() path)
        // never learn that this deferred descendant later swapped subtrees,
        // leaving them wired to a render subtree that Element-side is
        // already unmounted (see StatefulElement::performBuild()).
        if (child_ != old_child)
            onDescendantRenderObjectChanged();
    }

} // namespace systems::leal::campello_widgets
