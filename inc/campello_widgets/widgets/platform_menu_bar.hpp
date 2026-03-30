#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/platform_menu.hpp>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that configures the platform's native menu bar.
     *
     * PlatformMenuBar uses the platform's native APIs to construct and render
     * a menu described by a PlatformMenu/PlatformMenuItem hierarchy.
     *
     * This widget has no visual representation in the widget tree - it simply
     * returns its child from build(). All rendering, shortcuts, and event
     * handling for the menu is handled by the host platform.
     *
     * PlatformMenuBar is especially useful on macOS, where a system menu is
     * required for every application. Support for other platforms (Windows,
     * Linux) may vary.
     *
     * There can only be one active PlatformMenuBar at a time. If multiple
     * PlatformMenuBars are in the widget tree, the deepest one takes precedence.
     *
     * Example usage:
     * @code
     * class MyApp : public StatelessWidget {
     *     WidgetRef build(BuildContext& ctx) const override {
     *         return PlatformMenuBar::create({
     *             .menus = {
     *                 PlatformMenu::create("File", {
     *                     PlatformMenuItemLabel::create("Open...", "Cmd+O", []() {
     *                         // Open file dialog
     *                     }),
     *                     PlatformMenuItemSeparator::create(),
     *                     PlatformProvidedMenuItem::quit(),
     *                 }),
     *                 PlatformMenu::create("Edit", {
     *                     PlatformProvidedMenuItem::cut(),
     *                     PlatformProvidedMenuItem::copy(),
     *                     PlatformProvidedMenuItem::paste(),
     *                 }),
     *             },
     *             .child = Scaffold::create({ ... })
     *         });
     *     }
     * };
     * @endcode
     */
    class PlatformMenuBar : public StatelessWidget
    {
    public:
        /**
         * @brief The menus to display in the platform menu bar.
         *
         * On macOS, these appear in the system menu bar at the top of the screen.
         * The first menu typically contains the application name.
         */
        std::vector<PlatformMenuRef> menus;

        /**
         * @brief The child widget to display.
         *
         * This is the only content that PlatformMenuBar renders - the menu
         * bar itself is handled entirely by the platform.
         */
        WidgetRef child;

        PlatformMenuBar() = default;
        explicit PlatformMenuBar(WidgetRef c) : child(std::move(c)) {}
        PlatformMenuBar(std::vector<PlatformMenuRef> m, WidgetRef c)
            : menus(std::move(m)), child(std::move(c))
        {}

        WidgetRef build(BuildContext& context) const override;

        // Factory methods
        static std::shared_ptr<PlatformMenuBar> create(WidgetRef child);
        static std::shared_ptr<PlatformMenuBar> create(std::vector<PlatformMenuRef> menus, WidgetRef child);
    };

} // namespace systems::leal::campello_widgets
