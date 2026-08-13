#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <cstdint>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Renders the active PlatformMenuBar's menus as an in-window
     * menu bar, on platforms with no native OS menu bar.
     *
     * Place this widget explicitly wherever the app wants the bar to
     * appear (e.g. docked at the top of a Scaffold body), below a
     * PlatformMenuBar ancestor. It reads the ancestor's menus via
     * PlatformMenuBar::menusOf() and defers to
     * PlatformMenuDelegate::instance()->needsInWindowMenuBar():
     *
     * - Where the platform already draws a native menu bar (macOS,
     *   Windows), this widget renders nothing — the PlatformMenuBar
     *   ancestor already handles both display and keyboard shortcuts via
     *   the platform's own APIs.
     * - Where there is no native equivalent (Linux — X11/Wayland have no
     *   OS-level menu-bar concept), it renders a real menu bar built from
     *   the same PlatformMenu data, and also registers keyboard
     *   accelerators (PlatformMenuItemLabel::shortcut) through
     *   FocusManager::setGlobalKeyHandler() so they fire regardless of
     *   what currently has focus.
     *
     * @code
     * PlatformMenuBar::create({ ... },
     *     Scaffold::create({
     *         .body = Column::create({
     *             PlatformMenuBarView::create(),
     *             Expanded::create(restOfUi),
     *         }),
     *     }));
     * @endcode
     */
    class PlatformMenuBarView : public StatefulWidget
    {
    public:
        std::unique_ptr<StateBase> createState() const override;

        static std::shared_ptr<PlatformMenuBarView> create();
    };

    namespace detail
    {
        /**
         * @brief Parses a shortcut string ("Ctrl+Shift+S", "Cmd+O", ...)
         * into a KeyCode and modifier bitmask for matching against
         * KeyEvent — internal to PlatformMenuBarView, exposed here only
         * so it can be unit tested directly.
         *
         * "Cmd" is treated as an alias for Ctrl: Linux has no Cmd key, and
         * menu items are typically authored once with a single shortcut
         * string shared across platforms (macOS resolves "Cmd" natively
         * via NSMenuItem instead of going through this parser at all).
         *
         * Returns false if the string is empty, has an unrecognized
         * modifier, or names a key with no KeyCode equivalent (this
         * engine's KeyCode only covers letters, digits, and a fixed set of
         * named keys — no punctuation).
         */
        bool parseMenuShortcut(const std::string& shortcut, KeyCode& out_code, uint32_t& out_modifiers);
    }

} // namespace systems::leal::campello_widgets
