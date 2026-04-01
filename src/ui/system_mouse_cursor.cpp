#include <campello_widgets/ui/system_mouse_cursor.hpp>

namespace systems::leal::campello_widgets
{

    static CursorSetFn g_cursor_fn;

    void registerCursorHandler(CursorSetFn fn)
    {
        g_cursor_fn = std::move(fn);
    }

    void setSystemCursor(SystemMouseCursor cursor)
    {
        if (g_cursor_fn) g_cursor_fn(cursor);
    }

    void resetSystemCursor()
    {
        if (g_cursor_fn) g_cursor_fn(SystemMouseCursor::arrow);
    }

} // namespace systems::leal::campello_widgets
