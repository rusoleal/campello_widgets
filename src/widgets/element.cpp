#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/inherited_element.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/widget_inspector.hpp>
#include <campello_widgets/ui/debug_flags.hpp>

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
        mounted_ = true;
        if (parent_)
            inherited_widgets_ = parent_->inherited_widgets_;
        onMountInheritance();
        dirty_ = true;

        // Register with GlobalKey if present
        if (auto* gk = dynamic_cast<GlobalKey*>(widget_->key.get()))
            GlobalKey::_register(gk, this);

        rebuild();
    }

    const InheritedWidget* Element::getInheritedWidget(const std::type_info& type)
    {
        auto it = inherited_widgets_.find(std::type_index(type));
        if (it == inherited_widgets_.end()) return nullptr;
        InheritedElement* inh = it->second;
        inh->addDependent(this);
        return &static_cast<const InheritedWidget&>(inh->widget());
    }

    void Element::unmount()
    {
        // Unregister from GlobalKey if present
        if (auto* gk = dynamic_cast<GlobalKey*>(widget_->key.get()))
            GlobalKey::_unregister(gk);

        mounted_ = false;
        parent_  = nullptr;
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

        if (DebugFlags::printRebuildsEnabled)
        {
            std::cout << "[REBUILD] " << widget_->toStringShort() << "\n";
        }
        WidgetInspector::instance().recordRebuild(this);

        performBuild();
    }

    void Element::onDescendantRenderObjectChanged()
    {
        if (parent_) parent_->onDescendantRenderObjectChanged();
    }

    void Element::debugFillProperties(DiagnosticsPropertyBuilder& out) const
    {
        out.add(std::make_unique<StringProperty>("widget", widget_->toStringShort()));
        out.add(std::make_unique<FlagProperty>("dirty", dirty_, "dirty", "clean"));
    }

    std::vector<std::shared_ptr<DiagnosticsNode>> Element::debugDescribeChildren() const
    {
        std::vector<std::shared_ptr<DiagnosticsNode>> result;
        visitChildren([&](Element* child) {
            result.push_back(child->toDiagnosticsNode());
        });
        return result;
    }

    std::string Element::toStringShort() const
    {
        return widget_->toStringShort() + " Element";
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
            Key* old_key = child->widget().key.get();
            Key* new_key = new_widget->key.get();

            // Keys must match (or both be absent) to reuse the element.
            // If either side has a key and they differ, force recreation.
            const bool keys_match =
                (!old_key && !new_key) ||
                (old_key && new_key && new_key->equals(*old_key));

            if (keys_match && child->widget().widgetType() == new_widget->widgetType())
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
