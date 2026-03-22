#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that describes its UI entirely through a `build()` function.
     *
     * StatelessWidget has no mutable state of its own. Its appearance is determined
     * solely by the constructor arguments it receives from its parent widget.
     * The framework calls `build()` whenever the widget's configuration changes.
     *
     * **Usage:**
     * @code
     * class MyLabel : public StatelessWidget {
     * public:
     *     explicit MyLabel(std::string text) : text_(std::move(text)) {}
     *
     *     WidgetRef build(BuildContext& ctx) const override {
     *         return Text::create(text_);
     *     }
     * private:
     *     std::string text_;
     * };
     * @endcode
     */
    class StatelessWidget : public Widget
    {
    public:
        /**
         * @brief Describes the UI represented by this widget.
         *
         * The framework calls this method whenever this widget appears in the tree
         * and its configuration has changed. The returned widget becomes this
         * widget's single child in the tree.
         *
         * @param context The BuildContext for this widget's location in the tree.
         * @return A WidgetRef describing the subtree rooted at this widget.
         */
        virtual WidgetRef build(BuildContext& context) const = 0;

        std::shared_ptr<Element> createElement() const override;
    };

} // namespace systems::leal::campello_widgets
