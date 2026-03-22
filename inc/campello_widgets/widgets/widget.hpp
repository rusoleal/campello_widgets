#pragma once

#include <memory>
#include <typeinfo>

namespace systems::leal::campello_widgets
{

    class Element;

    /**
     * @brief Immutable description of a piece of UI.
     *
     * A Widget is a lightweight, immutable configuration object. It describes
     * *what* should be on screen but does not hold mutable state or render anything
     * itself. Widgets are cheap to create and discard — the framework reconciles
     * the new widget tree against the existing Element tree each frame.
     *
     * Every concrete widget must implement `createElement()` to produce the
     * appropriate Element subclass. Prefer subclassing `StatelessWidget` or
     * `StatefulWidget` over inheriting Widget directly.
     */
    class Widget : public std::enable_shared_from_this<Widget>
    {
    public:
        virtual ~Widget() = default;

        /**
         * @brief Creates the Element that manages this widget's lifecycle in the tree.
         *
         * Called by the framework the first time this widget type appears at a given
         * position in the tree. Subsequent renders reuse the existing Element and
         * call `Element::update()` instead.
         */
        virtual std::shared_ptr<Element> createElement() const = 0;

        /**
         * @brief Returns the dynamic type identity of this widget.
         *
         * Used during reconciliation to decide whether an existing Element can be
         * updated in-place (same type) or must be replaced (different type).
         */
        const std::type_info& widgetType() const noexcept { return typeid(*this); }
    };

    /** @brief Shared ownership handle to an immutable Widget. */
    using WidgetRef = std::shared_ptr<const Widget>;

    /** @brief Convenience alias for a brace-initialised child list.
     *
     *  Allows passing children inline without an explicit std::vector:
     *  @code
     *    make<Column>(WidgetList{a, b, c}, MainAxisAlignment::start)
     *  @endcode
     */
    using WidgetList = std::initializer_list<WidgetRef>;

} // namespace systems::leal::campello_widgets
