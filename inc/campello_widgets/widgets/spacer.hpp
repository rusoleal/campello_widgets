#pragma once

#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Creates an adjustable, empty spacer that takes up space
     * proportional to its flex value.
     *
     * Must be a child of a Flex (Row/Column). Subclasses `Expanded` directly
     * (rather than building one via a StatelessWidget) because Flex's
     * flex-detection is a `dynamic_cast<const Flexible*>` on the immediate
     * child widget reference in its `children` list (see `flex.cpp`) -- a
     * wrapper that only returned an `Expanded` from `build()` would not be
     * seen as flexible by that check.
     */
    class Spacer : public Expanded
    {
    public:
        Spacer() : Expanded(1, SizedBox::shrink()) {}
        explicit Spacer(int flex_value) : Expanded(flex_value, SizedBox::shrink()) {}
    };

} // namespace systems::leal::campello_widgets
