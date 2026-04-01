#pragma once

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief System cursor shapes for use with MouseRegion.
     */
    enum class SystemMouseCursor
    {
        arrow,      ///< Default arrow cursor.
        pointer,    ///< Pointing hand — use for clickable widgets.
        text,       ///< I-beam — use for editable text.
        forbidden,  ///< No-entry / not-allowed.
        resize_ns,  ///< North-south resize (vertical edges).
        resize_ew,  ///< East-west resize (horizontal edges).
    };

    using CursorSetFn = std::function<void(SystemMouseCursor)>;

    /**
     * @brief Registers the platform cursor-change implementation.
     *
     * Called once at application startup by the platform runner (e.g. run_app.mm).
     */
    void registerCursorHandler(CursorSetFn fn);

    /** @brief Changes the system cursor to the given shape. */
    void setSystemCursor(SystemMouseCursor cursor);

    /** @brief Resets the system cursor to the default arrow. */
    void resetSystemCursor();

} // namespace systems::leal::campello_widgets
