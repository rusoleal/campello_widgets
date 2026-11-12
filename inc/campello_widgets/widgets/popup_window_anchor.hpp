#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Shows `popup` (a menu, dropdown or tooltip) anchored below
     *        `child` while `open` is true (Phase 17 M7).
     *
     * Where the platform has popup windows (macOS, the web), the popup is a
     * borderless window of its own (WindowKind::popup), so it can extend
     * past the edges of the window it belongs to; it moves with that window.
     * Elsewhere (iOS, Android), or with `native = false`, it is an entry in
     * the window's Overlay at the same place, clipped to the window. Windows
     * opened with WindowController always have an Overlay; the main window
     * has one if the app puts it in its tree (as most do).
     *
     * Either way it is dismissed (on_dismiss) when the user presses anywhere
     * else in the window, presses Escape, or (native) the popup loses focus.
     * `on_dismiss` normally sets `open` to false; without it the popup closes
     * itself.
     *
     * @code
     * auto anchor = std::make_shared<PopupWindowAnchor>();
     * anchor->child      = button("Options", [this] { setState([&] { menu_open_ = true; }); });
     * anchor->popup      = buildMenu();
     * anchor->open       = menu_open_;
     * anchor->popup_size = {220.0f, 180.0f};
     * anchor->on_dismiss = [this] { setState([&] { menu_open_ = false; }); };
     * @endcode
     */
    class PopupWindowAnchor : public StatefulWidget
    {
    public:
        WidgetRef child;   ///< The anchor, laid out and painted as usual.
        WidgetRef popup;   ///< The popup's content, sized popup_size.
        bool      open = false;
        Size      popup_size{200.0f, 240.0f};      ///< Logical pixels.
        Offset    offset{0.0f, 4.0f};              ///< From the anchor's bottom-left corner.
        bool      native = true;                   ///< false: always the in-window Overlay.
        std::function<void()> on_dismiss;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
