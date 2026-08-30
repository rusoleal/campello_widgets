#pragma once

#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Propagates a default TextStyle down the widget tree.
     *
     * `Text`/`RichText` descendants that don't set an explicit style call
     * `DefaultTextStyle::of(context)` and merge it with their own (non-default)
     * fields via `TextStyle::merge()`, so a descendant's explicit style always
     * wins over the inherited default.
     *
     * @code
     * runApp(mw<DefaultTextStyle>(DefaultTextStyle{
     *     .style = TextStyle{}.withFontSize(16.0f),
     *     .child = mw<MyApp>(),
     * }));
     * @endcode
     */
    class DefaultTextStyle : public InheritedWidget
    {
    public:
        TextStyle style;

        DefaultTextStyle() = default;
        DefaultTextStyle(TextStyle s, WidgetRef c)
            : style(std::move(s))
        {
            this->child = std::move(c);
        }

        /**
         * @brief Looks up the nearest DefaultTextStyle ancestor and registers
         *        this element as a dependent so it rebuilds when it changes.
         *
         * @return The inherited style, or a default-constructed TextStyle if
         *         no DefaultTextStyle ancestor exists.
         */
        static TextStyle of(BuildContext& context);

        bool updateShouldNotify(const InheritedWidget& old_widget) const override;
    };

} // namespace systems::leal::campello_widgets
