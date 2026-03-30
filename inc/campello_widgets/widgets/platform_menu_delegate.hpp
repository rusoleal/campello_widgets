#pragma once

#include <campello_widgets/widgets/platform_menu.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Callback handle for menu item selection.
     *
     * When a platform menu item is selected, this handle is used to
     * invoke the appropriate callback.
     */
    using MenuCallbackId = uint64_t;

    /**
     * @brief Singleton delegate for platform menu bar management.
     *
     * PlatformMenuDelegate acts as the bridge between the widget tree
     * and the platform's native menu bar APIs. It maintains the current
     * menu structure and routes callbacks when menu items are selected.
     *
     * Each platform (macOS, Windows, Linux) provides its own implementation
     * that interfaces with native APIs.
     */
    class PlatformMenuDelegate
    {
    public:
        virtual ~PlatformMenuDelegate() = default;

        /**
         * @brief Returns the global PlatformMenuDelegate instance.
         *
         * The returned pointer is never null. On platforms without menu bar
         * support, this returns a no-op implementation.
         */
        static PlatformMenuDelegate* instance();

        /**
         * @brief Sets the global PlatformMenuDelegate instance.
         *
         * This is called by platform-specific initialization code to install
         * the appropriate implementation.
         *
         * @param delegate The delegate instance to use. If null, a no-op
         *                 implementation will be used.
         */
        static void setInstance(std::unique_ptr<PlatformMenuDelegate> delegate);

        /**
         * @brief Sets the menus to display in the platform menu bar.
         *
         * This method is called by PlatformMenuBar during build() to update
         * the native menu bar. The delegate is responsible for:
         * - Creating/updating native menu structures
         * - Registering keyboard shortcuts
         * - Routing callbacks when items are selected
         *
         * @param menus The list of menus to display.
         */
        virtual void setMenus(const std::vector<PlatformMenuRef>& menus) = 0;

        /**
         * @brief Clears the platform menu bar.
         *
         * Removes all custom menus and restores the default state.
         */
        virtual void clearMenus() = 0;

        /**
         * @brief Invokes a callback by its ID.
         *
         * Called by platform-specific code when a menu item is selected.
         *
         * @param callback_id The ID of the callback to invoke.
         */
        void invokeCallback(MenuCallbackId callback_id);

    protected:
        /**
         * @brief Registers a callback and returns its ID.
         *
         * Used by implementations to register callbacks before passing
         * menu structures to native APIs.
         *
         * @param callback The callback function to register.
         * @return A unique ID for this callback.
         */
        MenuCallbackId registerCallback(std::function<void()> callback);

        /**
         * @brief Clears all registered callbacks.
         */
        void clearCallbacks();

    private:
        static std::unique_ptr<PlatformMenuDelegate> instance_;
        static std::mutex instance_mutex_;

        std::mutex callbacks_mutex_;
        std::vector<std::function<void()>> callbacks_;
        MenuCallbackId next_callback_id_ = 1;
    };

    /**
     * @brief No-op implementation of PlatformMenuDelegate.
     *
     * Used on platforms that don't support menu bars (iOS, Android)
     * or when no platform implementation is available.
     */
    class NoOpPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        void setMenus(const std::vector<PlatformMenuRef>& /*menus*/) override {}
        void clearMenus() override {}
    };

} // namespace systems::leal::campello_widgets
