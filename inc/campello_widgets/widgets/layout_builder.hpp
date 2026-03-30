#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Builds a widget tree based on the parent's layout constraints.
     *
     * The `builder` callback is invoked with the actual `BoxConstraints` imposed
     * by the parent, allowing the widget to adapt its layout responsively.
     *
     * Constraints are captured during the layout pass and trigger a synchronous
     * rebuild if they differ from the constraints used for the previous build.
     *
     * @code
     * auto lb = std::make_shared<LayoutBuilder>();
     * lb->builder = [](BuildContext&, BoxConstraints c) {
     *     if (c.max_width < 600.0f) {
     *         return std::make_shared<Column>(...); // narrow layout
     *     }
     *     return std::make_shared<Row>(...);        // wide layout
     * };
     * @endcode
     */
    class LayoutBuilder : public StatefulWidget
    {
    public:
        std::function<WidgetRef(BuildContext&, BoxConstraints)> builder;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
