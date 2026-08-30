#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief Which baseline metric to measure/align against.
     *
     * Matches Flutter's `TextBaseline` enum. Only `alphabetic` is currently
     * distinguished by `RenderBox::computeDistanceToActualBaseline()`
     * overrides -- `ideographic` uses the same approximation for now.
     */
    enum class TextBaseline
    {
        alphabetic,
        ideographic,
    };

} // namespace systems::leal::campello_widgets
