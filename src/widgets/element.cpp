#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/inherited_element.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/widget_inspector.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

namespace systems::leal::campello_widgets
{

    std::vector<Element*> Element::dirty_elements_;
    std::unordered_set<const Element*> Element::s_alive_;

    Element::Element(WidgetRef widget)
        : widget_(std::move(widget))
    {
        s_alive_.insert(this);
    }

    Element::~Element()
    {
        s_alive_.erase(this);
    }

    bool Element::isAlive(const Element* obj) noexcept
    {
        return obj && s_alive_.find(obj) != s_alive_.end();
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

#ifndef NDEBUG
        widget_->debugValidate();
#endif

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
        ThreadChecker::instance().assertOnBoundThread("Element::markNeedsBuild");
        if (dirty_) return;
        dirty_ = true;
        dirty_elements_.push_back(this);
        FrameScheduler::scheduleFrame();

        // In test environments where no frame callback is registered, rebuild
        // synchronously so tests that inspect the tree after setState() continue
        // to work. In real apps the Renderer drives buildScope() before layout.
        if (!FrameScheduler::hasCallback())
            buildScope();
    }

    void Element::buildScope()
    {
        while (true)
        {
            auto dirty = std::move(dirty_elements_);
            dirty_elements_.clear();
            if (dirty.empty()) break;
            for (Element* e : dirty)
            {
                if (isAlive(e) && e->mounted_)
                    e->rebuild();
            }
        }
    }

    void Element::rebuild()
    {
        if (!dirty_ || building_) return;
        building_ = true;
        dirty_    = false;

        if (DebugFlags::printRebuildsEnabled)
        {
            std::cout << "[REBUILD] " << widget_->toStringShort() << "\n";
        }
        WidgetInspector::instance().recordRebuild(this);

        performBuild();
        building_ = false;
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
                // Skip the rebuild entirely when it's literally the same
                // immutable Widget instance — mirrors Flutter's identical
                // check in updateChild. Without this, reusing a cached
                // WidgetRef (e.g. OverlayState rebuilding its Stack to add
                // a sibling entry) forces every other untouched subtree to
                // re-run update() too, which can be destructive further
                // down (e.g. TreeViewElement::update() unconditionally
                // unmounts and remounts all rows).
                if (&child->widget() != new_widget.get())
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
