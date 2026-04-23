#include <campello_widgets/diagnostics/widget_inspector.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_stack.hpp>

#include <iomanip>
#include <sstream>

namespace systems::leal::campello_widgets
{

    WidgetInspector& WidgetInspector::instance()
    {
        static WidgetInspector inst;
        return inst;
    }

    void WidgetInspector::setRootElement(std::weak_ptr<Element> root)
    {
        root_element_ = std::move(root);
    }

    std::shared_ptr<DiagnosticsNode> WidgetInspector::getWidgetTree() const
    {
        if (auto root = root_element_.lock())
            return getWidgetTree(*root);
        return std::make_shared<DiagnosticsNode>("[no root element]");
    }

    std::shared_ptr<DiagnosticsNode> WidgetInspector::getRenderObjectTree() const
    {
        auto* renderer = detail::currentRenderer();
        if (!renderer) return std::make_shared<DiagnosticsNode>("[no renderer]");
        // Root render box is stored in renderer but not directly accessible.
        // For now, try to find it through the root element's descendant.
        if (auto root = root_element_.lock())
        {
            auto* roe = root->findDescendantRenderObjectElement();
            if (roe && roe->renderObject())
                return getRenderObjectTree(*roe->renderObject());
        }
        return std::make_shared<DiagnosticsNode>("[no render tree]");
    }

    std::shared_ptr<DiagnosticsNode> WidgetInspector::getWidgetTree(Element& root) const
    {
        return root.toDiagnosticsNode();
    }

    std::shared_ptr<DiagnosticsNode> WidgetInspector::getRenderObjectTree(RenderObject& root) const
    {
        auto node = root.toDiagnosticsNode();
        // Recurse into RenderBox children via visitRenderChildren
        if (auto* box = dynamic_cast<RenderBox*>(&root))
        {
            box->visitRenderChildren([&](RenderBox* child) {
                node->children.push_back(getRenderObjectTree(*child));
            });
        }
        return node;
    }

    void WidgetInspector::dumpWidgetTree(std::ostream& out) const
    {
        out << "\n========== Widget Tree ==========\n";
        out << getWidgetTree()->toStringDeep();
        out << "\n=================================\n\n";
    }

    void WidgetInspector::dumpRenderObjectTree(std::ostream& out) const
    {
        out << "\n========== Render Object Tree ==========\n";
        out << getRenderObjectTree()->toStringDeep();
        out << "\n========================================\n\n";
    }

    void WidgetInspector::setSelectedElement(Element* element)
    {
        selected_element_ = element;
    }

    void WidgetInspector::setSelectModeEnabled(bool enabled)
    {
        select_mode_enabled_ = enabled;
    }

    void WidgetInspector::recordRebuild(Element* element)
    {
        if (!element) return;
        ++rebuild_counts_[element];
    }

    void WidgetInspector::resetRebuildCounters()
    {
        rebuild_counts_.clear();
    }

    int WidgetInspector::rebuildCount(Element* element) const
    {
        auto it = rebuild_counts_.find(element);
        return (it != rebuild_counts_.end()) ? it->second : 0;
    }

    RenderBox* WidgetInspector::selectedRenderBox() const
    {
        if (!selected_element_) return nullptr;
        auto* roe = selected_element_->findDescendantRenderObjectElement();
        if (!roe) return nullptr;
        return dynamic_cast<RenderBox*>(roe->renderObject());
    }

} // namespace systems::leal::campello_widgets
