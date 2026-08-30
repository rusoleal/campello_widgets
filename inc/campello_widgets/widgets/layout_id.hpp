#pragma once

#include <string>
#include <campello_widgets/widgets/stateless_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Tags a child of `CustomMultiChildLayout` with an id its
     * `MultiChildLayoutDelegate` uses to look it up.
     *
     * Transparent -- `build()` just returns `child`, matching `Positioned`'s
     * shape for `Stack`.
     *
     * Matches Flutter's `LayoutId` widget.
     */
    class LayoutId : public StatelessWidget
    {
    public:
        std::string id;
        WidgetRef   child;

        LayoutId() = default;
        explicit LayoutId(std::string i, WidgetRef c) : id(std::move(i)), child(std::move(c)) {}

        WidgetRef build(BuildContext&) const override { return child; }
    };

} // namespace systems::leal::campello_widgets
