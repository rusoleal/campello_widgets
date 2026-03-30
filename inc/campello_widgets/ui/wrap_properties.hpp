#pragma once

namespace systems::leal::campello_widgets
{

    /** @brief How children within a single Wrap run are aligned on the main axis. */
    enum class WrapAlignment
    {
        start,
        end,
        center,
        space_between,
        space_around,
        space_evenly,
    };

    /** @brief How children within a run are aligned on the cross axis. */
    enum class WrapCrossAlignment
    {
        start,
        end,
        center,
    };

    /** @brief How the runs themselves are aligned on the cross axis of the Wrap. */
    enum class WrapRunAlignment
    {
        start,
        end,
        center,
        space_between,
        space_around,
        space_evenly,
    };

} // namespace systems::leal::campello_widgets
