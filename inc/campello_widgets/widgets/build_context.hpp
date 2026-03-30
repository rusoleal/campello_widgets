#pragma once

#include <typeinfo>

namespace systems::leal::campello_widgets
{

    class Widget;
    class InheritedWidget;

    /**
     * @brief Handle to a widget's location in the tree, passed to `build()` calls.
     *
     * BuildContext is the interface through which widgets can query information
     * about their position in the widget tree (e.g. inherited data, theme,
     * media query). The concrete implementation is `Element` — the BuildContext
     * pointer passed to `build()` is always the widget's own Element.
     *
     * Do not store a BuildContext beyond the lifetime of the `build()` call that
     * supplied it.
     */
    class BuildContext
    {
    public:
        virtual ~BuildContext() = default;

        /** @brief The widget currently associated with this context. */
        virtual const Widget& widget() const = 0;

        /**
         * @brief Finds the nearest ancestor InheritedWidget of type T and
         *        registers this element as a dependent.
         *
         * The calling element will be rebuilt whenever T's data changes
         * (i.e. when `InheritedWidget::updateShouldNotify` returns true).
         *
         * Returns nullptr if no ancestor of type T exists.
         *
         * @code
         * const MyTheme* theme = ctx.dependOnInheritedWidgetOfExactType<MyTheme>();
         * @endcode
         */
        template<typename T>
        const T* dependOnInheritedWidgetOfExactType()
        {
            return static_cast<const T*>(getInheritedWidget(typeid(T)));
        }

    protected:
        /**
         * @brief Looks up the nearest ancestor InheritedWidget matching `type`,
         *        registers the caller as a dependent, and returns it.
         *        Returns nullptr if not found.
         */
        virtual const InheritedWidget* getInheritedWidget(const std::type_info& type) = 0;
    };

} // namespace systems::leal::campello_widgets
