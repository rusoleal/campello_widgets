#pragma once

#include <campello_widgets/widgets/widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Base class for widgets that propagate data down the widget tree.
     *
     * An InheritedWidget makes data available to all descendants without
     * explicit parameter passing. Descendants subscribe to it via:
     *
     * @code
     * const MyTheme* theme =
     *     ctx.dependOnInheritedWidgetOfExactType<MyTheme>();
     * @endcode
     *
     * When the inherited widget is updated (i.e. the parent rebuilds and
     * produces a new instance), `updateShouldNotify()` is called. If it
     * returns true, all registered dependents are marked dirty and rebuilt.
     *
     * **Usage:**
     * @code
     * class MyTheme : public InheritedWidget {
     * public:
     *     Color primary;
     *
     *     bool updateShouldNotify(const InheritedWidget& old) const override {
     *         return static_cast<const MyTheme&>(old).primary != primary;
     *     }
     * };
     * @endcode
     */
    class InheritedWidget : public Widget
    {
    public:
        /** @brief The single child widget that sits below this inherited widget. */
        WidgetRef child;

        /**
         * @brief Called by the framework when the parent rebuilds and produces
         *        a new widget of this type at the same position.
         *
         * Return true if the data carried by the new widget is different from
         * the old one and dependents should be rebuilt. Return false to skip
         * unnecessary rebuilds.
         *
         * @param old_widget The previous widget at this position.
         */
        virtual bool updateShouldNotify(const InheritedWidget& old_widget) const = 0;

        std::shared_ptr<Element> createElement() const override;
    };

} // namespace systems::leal::campello_widgets
