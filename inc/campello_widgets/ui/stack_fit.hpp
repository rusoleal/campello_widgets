#pragma once

namespace systems::leal::campello_widgets
{

    /** @brief How non-positioned children in a Stack are sized. */
    enum class StackFit
    {
        loose,       ///< Children may be at most as large as the stack.
        expand,      ///< Children are forced to be as large as the stack.
        passthrough, ///< Children are given the same constraints as the stack.
    };

} // namespace systems::leal::campello_widgets
