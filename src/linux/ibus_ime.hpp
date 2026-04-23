#pragma once

#include <string>
#include <memory>
#include <functional>

// Forward declarations for libdbus
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef int DBusHandlerResult;

namespace systems::leal::campello_widgets
{

    class TextInputManager;

    /**
     * @brief IBus D-Bus client for Linux IME support.
     *
     * Connects to the IBus daemon via the session D-Bus, creates an input
     * context, and routes composition events to the active TextInputManager.
     *
     * Typical lifecycle:
     *   1. create() — connect to IBus, create input context
     *   2. focusIn() / focusOut() — when text field gains/loses focus
     *   3. processKeyEvent() — forward X11 key events to IBus
     *   4. setCursorLocation() — position candidate window near cursor
     *   5. dispatchEvents() — pump D-Bus signals (UpdatePreeditText, CommitText)
     *   6. destroy() — disconnect and clean up
     */
    class IbusIme
    {
    public:
        IbusIme();
        ~IbusIme();

        // Non-copyable, non-movable
        IbusIme(const IbusIme&) = delete;
        IbusIme& operator=(const IbusIme&) = delete;

        /**
         * @brief Connect to IBus and create an input context.
         * @return true on success, false if IBus is not available.
         */
        bool create();

        /** @brief Disconnect from IBus and release resources. */
        void destroy();

        /** @brief Returns true if connected to IBus. */
        bool isActive() const { return conn_ != nullptr && !ic_path_.empty(); }

        /**
         * @brief Notify IBus that a text field has gained focus.
         *
         * This should be called when a TextField registers with TextInputManager.
         */
        void focusIn();

        /**
         * @brief Notify IBus that a text field has lost focus.
         */
        void focusOut();

        /**
         * @brief Forward an X11 key event to IBus for processing.
         *
         * IBus may consume the key for composition (e.g., dead keys, CJK input).
         *
         * @param keysym    X11 keysym (e.g., XK_a)
         * @param keycode   X11 keycode
         * @param state     X11 modifier mask (Shift, Control, etc.)
         * @return true if IBus consumed the key (app should ignore it),
         *         false if the key should be processed normally.
         */
        bool processKeyEvent(uint32_t keysym, uint32_t keycode, uint32_t state);

        /**
         * @brief Set the screen location of the text cursor.
         *
         * IBus uses this to position the candidate / preedit window.
         *
         * @param x Cursor x coordinate in screen pixels.
         * @param y Cursor y coordinate in screen pixels.
         * @param w Cursor width in pixels.
         * @param h Cursor height in pixels.
         */
        void setCursorLocation(int x, int y, int w, int h);

        /**
         * @brief Pump pending D-Bus signals and route them to TextInputManager.
         *
         * Call this regularly from the event loop (e.g., after processing X11
         * events).  This dispatches UpdatePreeditText, CommitText, etc.
         */
        void dispatchEvents();

    private:
        bool createInputContext();
        bool addSignalMatches();
        static DBusHandlerResult dbusFilter(DBusConnection* conn,
                                            DBusMessage* msg,
                                            void* user_data);
        void handleSignal(DBusConnection* conn, DBusMessage* msg);
        void handleUpdatePreeditText(DBusMessage* msg);
        void handleCommitText(DBusMessage* msg);
        void handleHidePreeditText();

        DBusConnection* conn_ = nullptr;
        std::string ic_path_;      // Input context object path
        bool has_focus_ = false;
    };

} // namespace systems::leal::campello_widgets
