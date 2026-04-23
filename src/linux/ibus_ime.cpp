#include "ibus_ime.hpp"

#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>

#include <dbus/dbus.h>
#include <cstring>
#include <iostream>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    static bool checkError(DBusError& err, const char* context)
    {
        if (dbus_error_is_set(&err))
        {
            std::cerr << "[IBus] " << context << ": " << err.message << "\n";
            dbus_error_free(&err);
            return false;
        }
        return true;
    }

    /**
     * @brief Extract the text string from an IBusText D-Bus struct.
     *
     * IBusText is serialized as a struct whose first member is the string.
     * We navigate to the first string element and return it.
     */
    static std::string extractIbusTextString(DBusMessageIter* iter)
    {
        std::string result;
        int arg_type = dbus_message_iter_get_arg_type(iter);
        if (arg_type == DBUS_TYPE_STRUCT || arg_type == DBUS_TYPE_VARIANT)
        {
            DBusMessageIter sub;
            dbus_message_iter_recurse(iter, &sub);
            // First element of IBusText is the string
            if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING)
            {
                const char* str = nullptr;
                dbus_message_iter_get_basic(&sub, &str);
                if (str) result = str;
            }
        }
        else if (arg_type == DBUS_TYPE_STRING)
        {
            const char* str = nullptr;
            dbus_message_iter_get_basic(iter, &str);
            if (str) result = str;
        }
        return result;
    }

    // -------------------------------------------------------------------------
    // IbusIme
    // -------------------------------------------------------------------------

    IbusIme::IbusIme() = default;

    IbusIme::~IbusIme()
    {
        destroy();
    }

    bool IbusIme::create()
    {
        if (conn_) return true; // Already connected

        DBusError err;
        dbus_error_init(&err);

        conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (!checkError(err, "dbus_bus_get") || !conn_)
        {
            std::cerr << "[IBus] Failed to connect to session bus.\n";
            conn_ = nullptr;
            return false;
        }

        // Take a reference since dbus_bus_get returns a shared connection
        dbus_connection_ref(conn_);

        if (!createInputContext())
        {
            destroy();
            return false;
        }

        if (!addSignalMatches())
        {
            destroy();
            return false;
        }

        std::cerr << "[IBus] IME connected: " << ic_path_ << "\n";
        return true;
    }

    void IbusIme::destroy()
    {
        if (!conn_) return;

        dbus_connection_remove_filter(conn_, dbusFilter, this);

        if (!ic_path_.empty())
        {
            // Remove signal matches
            std::string match =
                "type='signal',interface='org.freedesktop.IBus.InputContext',path='" +
                ic_path_ + "'";
            DBusError err;
            dbus_error_init(&err);
            dbus_bus_remove_match(conn_, match.c_str(), &err);
            dbus_error_free(&err);
        }

        dbus_connection_unref(conn_);
        conn_ = nullptr;
        ic_path_.clear();
        has_focus_ = false;
    }

    bool IbusIme::createInputContext()
    {
        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            "/org/freedesktop/IBus",
            "org.freedesktop.IBus",
            "CreateInputContext");
        if (!msg) return false;

        const char* name = "campello_widgets";
        dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &name,
            DBUS_TYPE_INVALID);

        DBusError err;
        dbus_error_init(&err);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            conn_, msg, 5000, &err);
        dbus_message_unref(msg);

        if (!checkError(err, "CreateInputContext") || !reply)
        {
            if (reply) dbus_message_unref(reply);
            return false;
        }

        const char* path = nullptr;
        if (dbus_message_get_args(reply, &err,
            DBUS_TYPE_OBJECT_PATH, &path,
            DBUS_TYPE_INVALID))
        {
            if (path) ic_path_ = path;
        }
        dbus_message_unref(reply);

        if (ic_path_.empty())
        {
            std::cerr << "[IBus] CreateInputContext returned empty path.\n";
            return false;
        }

        // Set capabilities: preedit + focus + surround_text
        msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            ic_path_.c_str(),
            "org.freedesktop.IBus.InputContext",
            "SetCapabilities");
        if (msg)
        {
            uint32_t caps = 0x07; // IBUS_CAP_PREEDIT | IBUS_CAP_FOCUS | IBUS_CAP_SURROUNDING_TEXT
            dbus_message_append_args(msg,
                DBUS_TYPE_UINT32, &caps,
                DBUS_TYPE_INVALID);
            dbus_connection_send(conn_, msg, nullptr);
            dbus_message_unref(msg);
        }

        return true;
    }

    bool IbusIme::addSignalMatches()
    {
        if (ic_path_.empty()) return false;

        std::string match =
            "type='signal',interface='org.freedesktop.IBus.InputContext',path='" +
            ic_path_ + "'";

        DBusError err;
        dbus_error_init(&err);
        dbus_bus_add_match(conn_, match.c_str(), &err);
        if (!checkError(err, "add_signal_match")) return false;

        dbus_connection_add_filter(conn_, dbusFilter, this, nullptr);
        return true;
    }

    DBusHandlerResult IbusIme::dbusFilter(DBusConnection* /*conn*/,
                                          DBusMessage* msg,
                                          void* user_data)
    {
        auto* self = static_cast<IbusIme*>(user_data);
        if (!self) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        if (dbus_message_is_signal(msg, "org.freedesktop.IBus.InputContext", nullptr))
        {
            const char* path = dbus_message_get_path(msg);
            if (path && self->ic_path_ == path)
            {
                self->handleSignal(self->conn_, msg);
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    void IbusIme::focusIn()
    {
        if (!conn_ || ic_path_.empty()) return;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            ic_path_.c_str(),
            "org.freedesktop.IBus.InputContext",
            "FocusIn");
        if (msg)
        {
            dbus_connection_send(conn_, msg, nullptr);
            dbus_message_unref(msg);
            has_focus_ = true;
        }
    }

    void IbusIme::focusOut()
    {
        if (!conn_ || ic_path_.empty()) return;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            ic_path_.c_str(),
            "org.freedesktop.IBus.InputContext",
            "FocusOut");
        if (msg)
        {
            dbus_connection_send(conn_, msg, nullptr);
            dbus_message_unref(msg);
            has_focus_ = false;
        }
    }

    bool IbusIme::processKeyEvent(uint32_t keysym, uint32_t keycode, uint32_t state)
    {
        if (!conn_ || ic_path_.empty()) return false;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            ic_path_.c_str(),
            "org.freedesktop.IBus.InputContext",
            "ProcessKeyEvent");
        if (!msg) return false;

        dbus_message_append_args(msg,
            DBUS_TYPE_UINT32, &keysym,
            DBUS_TYPE_UINT32, &keycode,
            DBUS_TYPE_UINT32, &state,
            DBUS_TYPE_INVALID);

        DBusError err;
        dbus_error_init(&err);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            conn_, msg, 5000, &err);
        dbus_message_unref(msg);

        if (!checkError(err, "ProcessKeyEvent") || !reply)
        {
            if (reply) dbus_message_unref(reply);
            return false;
        }

        dbus_bool_t consumed = FALSE;
        dbus_message_get_args(reply, &err,
            DBUS_TYPE_BOOLEAN, &consumed,
            DBUS_TYPE_INVALID);
        dbus_message_unref(reply);

        return consumed == TRUE;
    }

    void IbusIme::setCursorLocation(int x, int y, int w, int h)
    {
        if (!conn_ || ic_path_.empty()) return;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.IBus",
            ic_path_.c_str(),
            "org.freedesktop.IBus.InputContext",
            "SetCursorLocation");
        if (!msg) return;

        int32_t ix = x, iy = y, iw = w, ih = h;
        dbus_message_append_args(msg,
            DBUS_TYPE_INT32, &ix,
            DBUS_TYPE_INT32, &iy,
            DBUS_TYPE_INT32, &iw,
            DBUS_TYPE_INT32, &ih,
            DBUS_TYPE_INVALID);

        dbus_connection_send(conn_, msg, nullptr);
        dbus_message_unref(msg);
    }

    void IbusIme::dispatchEvents()
    {
        if (!conn_) return;

        // Read pending D-Bus data and dispatch (this triggers our filter)
        dbus_connection_read_write(conn_, 0);

        while (dbus_connection_get_dispatch_status(conn_) == DBUS_DISPATCH_DATA_REMAINS)
        {
            dbus_connection_dispatch(conn_);
        }
    }

    void IbusIme::handleSignal(DBusConnection* /*conn*/, DBusMessage* msg)
    {
        const char* member = dbus_message_get_member(msg);
        if (!member) return;

        if (std::strcmp(member, "UpdatePreeditText") == 0)
        {
            handleUpdatePreeditText(msg);
        }
        else if (std::strcmp(member, "CommitText") == 0)
        {
            handleCommitText(msg);
        }
        else if (std::strcmp(member, "HidePreeditText") == 0)
        {
            handleHidePreeditText();
        }
    }

    void IbusIme::handleUpdatePreeditText(DBusMessage* msg)
    {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(msg, &iter)) return;

        // First argument: IBusText struct
        std::string text = extractIbusTextString(&iter);
        if (text.empty()) return;

        auto* tim = TextInputManager::activeManager();
        if (!tim) return;

        if (!tim->isComposing())
        {
            tim->beginComposing();
        }
        tim->updateComposingText(text);
    }

    void IbusIme::handleCommitText(DBusMessage* msg)
    {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(msg, &iter)) return;

        std::string text = extractIbusTextString(&iter);
        if (text.empty()) return;

        auto* tim = TextInputManager::activeManager();
        if (!tim) return;

        if (tim->isComposing())
        {
            tim->commitComposing();
        }
        tim->insertText(text);
    }

    void IbusIme::handleHidePreeditText()
    {
        auto* tim = TextInputManager::activeManager();
        if (!tim) return;

        if (tim->isComposing())
        {
            tim->cancelComposing();
        }
    }

} // namespace systems::leal::campello_widgets
