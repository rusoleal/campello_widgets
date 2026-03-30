#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace systems::leal::campello_widgets
{

    class RenderObjectElement;
    class InheritedElement;
    class InheritedWidget;

    /**
     * @brief Mutable instantiation of a Widget in the live widget tree.
     *
     * While Widgets are immutable descriptions, Elements are the persistent,
     * mutable objects that the framework keeps alive across rebuilds. Each
     * Element:
     *  - Holds a reference to its current Widget configuration.
     *  - Implements BuildContext so it can be passed to `build()` calls.
     *  - Manages the lifecycle of its child Element(s).
     *  - Drives the reconciliation loop via `updateChild()`.
     *
     * Subclasses implement `performBuild()` to produce their child subtree.
     */
    class Element : public BuildContext
    {
    public:
        explicit Element(WidgetRef widget);
        virtual ~Element() = default;

        // ------------------------------------------------------------------
        // BuildContext interface
        // ------------------------------------------------------------------

        const Widget& widget() const override;

        /**
         * @brief Looks up the nearest ancestor InheritedWidget of the given type,
         *        registers this element as a dependent, and returns it.
         *
         *        Called internally by BuildContext::dependOnInheritedWidgetOfExactType.
         */
        const InheritedWidget* getInheritedWidget(const std::type_info& type) override;

        // ------------------------------------------------------------------
        // Lifecycle
        // ------------------------------------------------------------------

        /**
         * @brief Inserts this element into the tree under `parent`.
         *
         * Sets the parent pointer, marks the element dirty, then calls `rebuild()`
         * synchronously. Subclasses that need extra initialisation (e.g. calling
         * `State::initState()`) should override this and call the base last.
         */
        virtual void mount(Element* parent);

        /**
         * @brief Removes this element from the tree.
         *
         * Subclasses should clean up resources and call the base implementation.
         */
        virtual void unmount();

        /**
         * @brief Updates the widget configuration for an already-mounted element.
         *
         * Called when the parent rebuilds and produces a new widget of the same
         * type at this position. Stores the new widget and schedules a rebuild.
         */
        virtual void update(WidgetRef new_widget);

        // ------------------------------------------------------------------
        // Rebuild
        // ------------------------------------------------------------------

        /** @brief Marks this element as needing a rebuild on the next frame. */
        void markNeedsBuild();

        /**
         * @brief Synchronously rebuilds this element if it is dirty.
         *
         * Clears the dirty flag then delegates to `performBuild()`.
         */
        virtual void rebuild();

        // ------------------------------------------------------------------
        // Tree reconciliation
        // ------------------------------------------------------------------

        /**
         * @brief Reconciles a child element against a new widget configuration.
         *
         * - If `new_widget` is null and `child` exists → unmounts and returns nullptr.
         * - If types match → calls `child->update(new_widget)` and returns `child`.
         * - If types differ → unmounts `child`, creates a new element, mounts it.
         *
         * @param child      The existing child element (may be nullptr).
         * @param new_widget The new widget to reconcile against (may be nullptr).
         * @param parent     The parent element for newly mounted children.
         */
        static std::shared_ptr<Element> updateChild(
            std::shared_ptr<Element> child,
            WidgetRef                new_widget,
            Element*                 parent);

        // ------------------------------------------------------------------
        // Render object tree traversal
        // ------------------------------------------------------------------

        /**
         * @brief If this element IS a RenderObjectElement, returns `this`.
         *        Otherwise returns nullptr. Override in RenderObjectElement.
         */
        virtual RenderObjectElement* nearestRenderObjectElement() noexcept
        {
            return nullptr;
        }

        /**
         * @brief Returns this element's first direct child element, or nullptr.
         *
         * Override in element subclasses that hold a single child (StatelessElement,
         * StatefulElement, SingleChildRenderObjectElement) to enable tree traversal.
         */
        virtual Element* firstChildElement() const noexcept { return nullptr; }

        /**
         * @brief Walks down the element subtree to find the nearest RenderObjectElement.
         *
         * Returns the first RenderObjectElement reachable through `firstChildElement()`
         * chains. Used by SingleChildRenderObjectElement to wire child render objects.
         */
        RenderObjectElement* findDescendantRenderObjectElement() noexcept;

    protected:
        /**
         * @brief Hook called after inherited_widgets_ is copied from parent
         *        but before rebuild(). Override in InheritedElement to insert
         *        itself into the map so descendants can find it during the
         *        initial build.
         */
        virtual void onMountInheritance() {}

        /** @brief Produce (or update) this element's child subtree. */
        virtual void performBuild() = 0;

        WidgetRef widget_;
        Element*  parent_ = nullptr;
        bool      dirty_  = true;

        /**
         * @brief Map from widget type to the nearest ancestor InheritedElement of
         *        that type. Populated during mount() by copying the parent's map.
         *        InheritedElement adds itself during its own mount().
         */
        std::unordered_map<std::type_index, InheritedElement*> inherited_widgets_;
    };

} // namespace systems::leal::campello_widgets
