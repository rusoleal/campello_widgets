#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class PlatformMenu;
    class PlatformMenuItem;
    class PlatformMenuItemLabel;
    class PlatformMenuItemSeparator;
    class PlatformProvidedMenuItem;

    /**
     * @brief Base class for all platform menu items.
     *
     * PlatformMenuItem is the abstract base for all items that can appear
     * in a platform-native menu bar. This includes labeled items, separators,
     * and platform-provided standard items.
     */
    class PlatformMenuItem
    {
    public:
        virtual ~PlatformMenuItem() = default;

        /**
         * @brief Creates a deep copy of this menu item.
         *
         * Used internally when passing menu structures to the platform layer.
         */
        virtual std::unique_ptr<PlatformMenuItem> clone() const = 0;
    };

    /** @brief Shared handle to a menu item. */
    using PlatformMenuItemRef = std::shared_ptr<PlatformMenuItem>;

    /**
     * @brief A labeled menu item with optional keyboard shortcut.
     *
     * Represents a clickable menu item with text label, optional keyboard
     * shortcut, and callback function.
     */
    class PlatformMenuItemLabel : public PlatformMenuItem
    {
    public:
        /** @brief The display text for this menu item. */
        std::string label;

        /** @brief Optional keyboard shortcut (e.g., "Cmd+Q", "Ctrl+O").
         *
         * Format: "Mod+Key" where Mod can be Cmd, Ctrl, Alt, Shift
         * and Key is a single character or special key name.
         */
        std::string shortcut;

        /** @brief Callback invoked when the menu item is selected. */
        std::function<void()> on_selected;

        /** @brief Whether this item is enabled (clickable). */
        bool enabled = true;

        /** @brief Whether this item shows a checkmark (toggle state). */
        bool checked = false;

        PlatformMenuItemLabel() = default;
        explicit PlatformMenuItemLabel(std::string lbl)
            : label(std::move(lbl))
        {}
        PlatformMenuItemLabel(std::string lbl, std::function<void()> callback)
            : label(std::move(lbl)), on_selected(std::move(callback))
        {}
        PlatformMenuItemLabel(std::string lbl, std::string sc, std::function<void()> callback)
            : label(std::move(lbl)), shortcut(std::move(sc)), on_selected(std::move(callback))
        {}

        std::unique_ptr<PlatformMenuItem> clone() const override;

        // Factory methods for convenient creation
        static PlatformMenuItemRef create(std::string label);
        static PlatformMenuItemRef create(std::string label, std::function<void()> on_selected);
        static PlatformMenuItemRef create(std::string label, std::string shortcut, std::function<void()> on_selected);
        static PlatformMenuItemRef create(std::string label, std::string shortcut, bool checked, std::function<void()> on_selected);
    };

    /**
     * @brief A separator line between menu items.
     */
    class PlatformMenuItemSeparator : public PlatformMenuItem
    {
    public:
        std::unique_ptr<PlatformMenuItem> clone() const override;

        static PlatformMenuItemRef create();
    };

    /**
     * @brief A submenu containing nested menu items.
     */
    class PlatformMenuItemSubmenu : public PlatformMenuItem
    {
    public:
        /** @brief The display text for this submenu. */
        std::string label;

        /** @brief The items contained in this submenu. */
        std::vector<PlatformMenuItemRef> items;

        /** @brief Whether this submenu is enabled. */
        bool enabled = true;

        PlatformMenuItemSubmenu() = default;
        explicit PlatformMenuItemSubmenu(std::string lbl)
            : label(std::move(lbl))
        {}
        PlatformMenuItemSubmenu(std::string lbl, std::vector<PlatformMenuItemRef> itms)
            : label(std::move(lbl)), items(std::move(itms))
        {}

        std::unique_ptr<PlatformMenuItem> clone() const override;

        static PlatformMenuItemRef create(std::string label);
        static PlatformMenuItemRef create(std::string label, std::vector<PlatformMenuItemRef> items);
    };

    /**
     * @brief Identifiers for platform-provided menu items.
     *
     * These map to standard menu items provided by the operating system.
     * Using these ensures proper OS integration and localization.
     */
    enum class PlatformProvidedMenuItemType
    {
        // Application menu
        about,           ///< About this application
        preferences,     ///< Preferences/Settings
        hide,            ///< Hide application
        hide_others,     ///< Hide other applications
        show_all,        ///< Show all applications
        quit,            ///< Quit application

        // File menu
        new_file,        ///< New file/document
        open,            ///< Open file
        close,           ///< Close file/window
        save,            ///< Save
        save_as,         ///< Save as
        print,           ///< Print

        // Edit menu
        undo,            ///< Undo
        redo,            ///< Redo
        cut,             ///< Cut
        copy,            ///< Copy
        paste,           ///< Paste
        paste_and_match_style, ///< Paste with matching style
        delete_item,     ///< Delete
        select_all,      ///< Select all

        // View menu
        toggle_fullscreen, ///< Enter/exit fullscreen
        minimize,        ///< Minimize window
        zoom,            ///< Zoom window

        // Window menu
        bring_all_to_front, ///< Bring all windows to front
    };

    /**
     * @brief A menu item provided by the platform.
     *
     * Uses the operating system's standard menu item for common actions,
     * ensuring proper localization, shortcuts, and behavior.
     */
    class PlatformProvidedMenuItem : public PlatformMenuItem
    {
    public:
        /** @brief The type of platform-provided menu item. */
        PlatformProvidedMenuItemType type;

        /** @brief Whether this item is enabled. */
        bool enabled = true;

        explicit PlatformProvidedMenuItem(PlatformProvidedMenuItemType t)
            : type(t)
        {}

        std::unique_ptr<PlatformMenuItem> clone() const override;

        // Convenience factory methods
        static PlatformMenuItemRef about() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::about); }
        static PlatformMenuItemRef preferences() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::preferences); }
        static PlatformMenuItemRef hide() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::hide); }
        static PlatformMenuItemRef hide_others() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::hide_others); }
        static PlatformMenuItemRef show_all() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::show_all); }
        static PlatformMenuItemRef quit() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::quit); }
        static PlatformMenuItemRef new_file() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::new_file); }
        static PlatformMenuItemRef open() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::open); }
        static PlatformMenuItemRef close() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::close); }
        static PlatformMenuItemRef save() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::save); }
        static PlatformMenuItemRef save_as() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::save_as); }
        static PlatformMenuItemRef print() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::print); }
        static PlatformMenuItemRef undo() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::undo); }
        static PlatformMenuItemRef redo() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::redo); }
        static PlatformMenuItemRef cut() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::cut); }
        static PlatformMenuItemRef copy() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::copy); }
        static PlatformMenuItemRef paste() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::paste); }
        static PlatformMenuItemRef paste_and_match_style() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::paste_and_match_style); }
        static PlatformMenuItemRef delete_item() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::delete_item); }
        static PlatformMenuItemRef select_all() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::select_all); }
        static PlatformMenuItemRef toggle_fullscreen() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::toggle_fullscreen); }
        static PlatformMenuItemRef minimize() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::minimize); }
        static PlatformMenuItemRef zoom() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::zoom); }
        static PlatformMenuItemRef bring_all_to_front() { return std::make_shared<PlatformProvidedMenuItem>(PlatformProvidedMenuItemType::bring_all_to_front); }
    };

    /**
     * @brief A top-level menu in the platform menu bar.
     *
     * Represents a menu that appears in the menu bar with a label and
     * a list of menu items.
     */
    class PlatformMenu
    {
    public:
        /** @brief The display label for this menu. */
        std::string label;

        /** @brief The items in this menu. */
        std::vector<PlatformMenuItemRef> items;

        PlatformMenu() = default;
        explicit PlatformMenu(std::string lbl)
            : label(std::move(lbl))
        {}
        PlatformMenu(std::string lbl, std::vector<PlatformMenuItemRef> itms)
            : label(std::move(lbl)), items(std::move(itms))
        {}

        // Factory methods
        static std::shared_ptr<PlatformMenu> create(std::string label);
        static std::shared_ptr<PlatformMenu> create(std::string label, std::vector<PlatformMenuItemRef> items);
    };

    /** @brief Shared handle to a platform menu. */
    using PlatformMenuRef = std::shared_ptr<PlatformMenu>;

} // namespace systems::leal::campello_widgets
