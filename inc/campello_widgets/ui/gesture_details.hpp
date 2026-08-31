#pragma once

#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Value types passed to GestureDetector's granular callbacks.
     *
     * Mirrors Flutter's gestures/{tap,drag,scale,...}_details.dart structs. Not all of these
     * are wired into GestureDetector's public API yet -- they're defined
     * together up front so later gesture-recognizer waves don't keep
     * reshaping this header.
     */

    /**
     * @brief Which position seeds a drag family's *StartDetails (Flutter's
     * DragStartBehavior). `down` (the default, matching this codebase's
     * pre-existing behavior) uses the original pointer-down position;
     * `start` uses the position at the moment slop was actually exceeded.
     */
    enum class DragStartBehavior
    {
        down,
        start,
    };

    struct TapDownDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct TapUpDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct LongPressDownDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct LongPressStartDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct LongPressMoveUpdateDetails
    {
        Offset global_position;
        Offset local_position;
        Offset offset_from_origin;
    };

    struct LongPressEndDetails
    {
        Offset global_position;
        Offset local_position;
        Offset velocity;
    };

    struct DragDownDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct DragStartDetails
    {
        Offset global_position;
        Offset local_position;
    };

    struct DragUpdateDetails
    {
        Offset delta;
        Offset global_position;
        Offset local_position;
    };

    struct DragEndDetails
    {
        Offset velocity;
        float  primary_velocity = 0.0f;
    };

    struct ScaleStartDetails
    {
        Offset focal_point;
        Offset local_focal_point;
        int    pointer_count = 0;
    };

    struct ScaleUpdateDetails
    {
        Offset focal_point;
        Offset local_focal_point;
        int    pointer_count     = 0;
        float  scale             = 1.0f;
        float  horizontal_scale  = 1.0f;
        float  vertical_scale    = 1.0f;
        float  rotation          = 0.0f;
    };

    struct ScaleEndDetails
    {
        Offset velocity;
        int    pointer_count = 0;
    };

    /// pressure is normalized 0.0-1.0. See ForcePressGestureRecognizer's doc
    /// comment for the device-support caveat -- this only ever fires for
    /// PointerDeviceKind::stylus in this codebase.
    struct ForcePressDetails
    {
        Offset global_position;
        Offset local_position;
        float  pressure = 0.0f;
    };

} // namespace systems::leal::campello_widgets
