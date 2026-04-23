#pragma once

#include <campello_widgets/diagnostics/diagnosticable.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Central service for inspecting widget, element, and render trees.
     *
     * Provides tree snapshots, selection tracking, rebuild counting, and
     * console dumps. Equivalent to Flutter's WidgetInspectorService.
     *
     * Usage:
     * @code
     * WidgetInspector::instance().dumpWidgetTree();
     * WidgetInspector::instance().dumpRenderObjectTree();
     * @endcode
     */
    class WidgetInspector
    {
    public:
        static WidgetInspector& instance();

        // ------------------------------------------------------------------
        // Root registration
        // ------------------------------------------------------------------

        /**
         * @brief Registers the root element of the application.
         *
         * Called automatically by runApp() on each platform. Needed for
         * tree dumps that don't take an explicit root.
         */
        void setRootElement(std::weak_ptr<Element> root);

        // ------------------------------------------------------------------
        // Tree snapshots
        // ------------------------------------------------------------------

        std::shared_ptr<DiagnosticsNode> getWidgetTree() const;
        std::shared_ptr<DiagnosticsNode> getRenderObjectTree() const;

        std::shared_ptr<DiagnosticsNode> getWidgetTree(Element& root) const;
        std::shared_ptr<DiagnosticsNode> getRenderObjectTree(RenderObject& root) const;

        // ------------------------------------------------------------------
        // Console dumps
        // ------------------------------------------------------------------

        void dumpWidgetTree(std::ostream& out = std::cout) const;
        void dumpRenderObjectTree(std::ostream& out = std::cout) const;

        // ------------------------------------------------------------------
        // Selection
        // ------------------------------------------------------------------

        void setSelectedElement(Element* element);
        Element* selectedElement() const noexcept { return selected_element_; }

        void setSelectModeEnabled(bool enabled);
        bool isSelectModeEnabled() const noexcept { return select_mode_enabled_; }

        // ------------------------------------------------------------------
        // Rebuild tracking
        // ------------------------------------------------------------------

        void recordRebuild(Element* element);
        void resetRebuildCounters();
        int rebuildCount(Element* element) const;

        // ------------------------------------------------------------------
        // Selection highlight
        // ------------------------------------------------------------------

        /** @brief Returns the render box currently selected, or nullptr. */
        RenderBox* selectedRenderBox() const;

    private:
        WidgetInspector() = default;

        std::weak_ptr<Element> root_element_;
        Element* selected_element_ = nullptr;
        bool select_mode_enabled_ = false;
        std::unordered_map<Element*, int> rebuild_counts_;
    };

} // namespace systems::leal::campello_widgets
