#pragma once

namespace systems::leal::campello_widgets
{

    class Widget;

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
    };

} // namespace systems::leal::campello_widgets
