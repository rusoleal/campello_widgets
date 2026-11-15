#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND

#include "wayland_window.hpp"

#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>

#include <campello_gpu/platform/linux_surface.hpp>

#include "ibus_ime.hpp"
#include "wayland_clipboard.hpp"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <libdecor.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-dialog-v1-client-protocol.h"

#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace Widgets = ::systems::leal::campello_widgets;
namespace GPU     = ::systems::leal::campello_gpu;

using Widgets::wayland::WaylandWindow;

// ============================================================================
// Key translation
// ============================================================================

static Widgets::KeyCode keysymToKeyCode(xkb_keysym_t keysym)
{
    switch (keysym) {
        case XKB_KEY_a: case XKB_KEY_A: return Widgets::KeyCode::a;
        case XKB_KEY_b: case XKB_KEY_B: return Widgets::KeyCode::b;
        case XKB_KEY_c: case XKB_KEY_C: return Widgets::KeyCode::c;
        case XKB_KEY_d: case XKB_KEY_D: return Widgets::KeyCode::d;
        case XKB_KEY_e: case XKB_KEY_E: return Widgets::KeyCode::e;
        case XKB_KEY_f: case XKB_KEY_F: return Widgets::KeyCode::f;
        case XKB_KEY_g: case XKB_KEY_G: return Widgets::KeyCode::g;
        case XKB_KEY_h: case XKB_KEY_H: return Widgets::KeyCode::h;
        case XKB_KEY_i: case XKB_KEY_I: return Widgets::KeyCode::i;
        case XKB_KEY_j: case XKB_KEY_J: return Widgets::KeyCode::j;
        case XKB_KEY_k: case XKB_KEY_K: return Widgets::KeyCode::k;
        case XKB_KEY_l: case XKB_KEY_L: return Widgets::KeyCode::l;
        case XKB_KEY_m: case XKB_KEY_M: return Widgets::KeyCode::m;
        case XKB_KEY_n: case XKB_KEY_N: return Widgets::KeyCode::n;
        case XKB_KEY_o: case XKB_KEY_O: return Widgets::KeyCode::o;
        case XKB_KEY_p: case XKB_KEY_P: return Widgets::KeyCode::p;
        case XKB_KEY_q: case XKB_KEY_Q: return Widgets::KeyCode::q;
        case XKB_KEY_r: case XKB_KEY_R: return Widgets::KeyCode::r;
        case XKB_KEY_s: case XKB_KEY_S: return Widgets::KeyCode::s;
        case XKB_KEY_t: case XKB_KEY_T: return Widgets::KeyCode::t;
        case XKB_KEY_u: case XKB_KEY_U: return Widgets::KeyCode::u;
        case XKB_KEY_v: case XKB_KEY_V: return Widgets::KeyCode::v;
        case XKB_KEY_w: case XKB_KEY_W: return Widgets::KeyCode::w;
        case XKB_KEY_x: case XKB_KEY_X: return Widgets::KeyCode::x;
        case XKB_KEY_y: case XKB_KEY_Y: return Widgets::KeyCode::y;
        case XKB_KEY_z: case XKB_KEY_Z: return Widgets::KeyCode::z;
        case XKB_KEY_0: return Widgets::KeyCode::digit_0;
        case XKB_KEY_1: return Widgets::KeyCode::digit_1;
        case XKB_KEY_2: return Widgets::KeyCode::digit_2;
        case XKB_KEY_3: return Widgets::KeyCode::digit_3;
        case XKB_KEY_4: return Widgets::KeyCode::digit_4;
        case XKB_KEY_5: return Widgets::KeyCode::digit_5;
        case XKB_KEY_6: return Widgets::KeyCode::digit_6;
        case XKB_KEY_7: return Widgets::KeyCode::digit_7;
        case XKB_KEY_8: return Widgets::KeyCode::digit_8;
        case XKB_KEY_9: return Widgets::KeyCode::digit_9;
        case XKB_KEY_space:      return Widgets::KeyCode::space;
        case XKB_KEY_Return:     return Widgets::KeyCode::enter;
        case XKB_KEY_Tab:        return Widgets::KeyCode::tab;
        case XKB_KEY_BackSpace:  return Widgets::KeyCode::backspace;
        case XKB_KEY_Escape:     return Widgets::KeyCode::escape;
        case XKB_KEY_Delete:     return Widgets::KeyCode::delete_forward;
        case XKB_KEY_Left:       return Widgets::KeyCode::left;
        case XKB_KEY_Right:      return Widgets::KeyCode::right;
        case XKB_KEY_Up:         return Widgets::KeyCode::up;
        case XKB_KEY_Down:       return Widgets::KeyCode::down;
        case XKB_KEY_Home:       return Widgets::KeyCode::home;
        case XKB_KEY_End:        return Widgets::KeyCode::end;
        case XKB_KEY_Page_Up:    return Widgets::KeyCode::page_up;
        case XKB_KEY_Page_Down:  return Widgets::KeyCode::page_down;
        case XKB_KEY_Shift_L:    return Widgets::KeyCode::left_shift;
        case XKB_KEY_Shift_R:    return Widgets::KeyCode::right_shift;
        case XKB_KEY_Control_L:  return Widgets::KeyCode::left_ctrl;
        case XKB_KEY_Control_R:  return Widgets::KeyCode::right_ctrl;
        case XKB_KEY_Alt_L:      return Widgets::KeyCode::left_alt;
        case XKB_KEY_Alt_R:      return Widgets::KeyCode::right_alt;
        case XKB_KEY_Super_L:    return Widgets::KeyCode::left_meta;
        case XKB_KEY_Super_R:    return Widgets::KeyCode::right_meta;
        case XKB_KEY_Caps_Lock:  return Widgets::KeyCode::caps_lock;
        case XKB_KEY_F1:  return Widgets::KeyCode::f1;
        case XKB_KEY_F2:  return Widgets::KeyCode::f2;
        case XKB_KEY_F3:  return Widgets::KeyCode::f3;
        case XKB_KEY_F4:  return Widgets::KeyCode::f4;
        case XKB_KEY_F5:  return Widgets::KeyCode::f5;
        case XKB_KEY_F6:  return Widgets::KeyCode::f6;
        case XKB_KEY_F7:  return Widgets::KeyCode::f7;
        case XKB_KEY_F8:  return Widgets::KeyCode::f8;
        case XKB_KEY_F9:  return Widgets::KeyCode::f9;
        case XKB_KEY_F10: return Widgets::KeyCode::f10;
        case XKB_KEY_F11: return Widgets::KeyCode::f11;
        case XKB_KEY_F12: return Widgets::KeyCode::f12;
        default:            return Widgets::KeyCode::unknown;
    }
}

static uint32_t xkbStateToKeyModifiers(struct xkb_state* state)
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (!state) return mods;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::shift;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,  XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::ctrl;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,   XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::alt;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,  XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::meta;
    return mods;
}

static bool isNavigationOrSpecialKey(Widgets::KeyCode k)
{
    using K = Widgets::KeyCode;
    switch (k) {
        case K::left: case K::right: case K::up: case K::down:
        case K::home: case K::end: case K::page_up: case K::page_down:
        case K::escape: case K::tab: case K::enter: case K::backspace: case K::delete_forward:
        case K::f1: case K::f2: case K::f3: case K::f4: case K::f5: case K::f6:
        case K::f7: case K::f8: case K::f9: case K::f10: case K::f11: case K::f12:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Process-level state
// ============================================================================

namespace
{
    struct Host
    {
        wl_display*       display    = nullptr;
        wl_registry*      registry   = nullptr;
        wl_compositor*    compositor = nullptr;
        wl_shm*           shm        = nullptr;
        wl_seat*          seat       = nullptr;
        wl_pointer*       pointer    = nullptr;
        wl_keyboard*      keyboard   = nullptr;
        xdg_wm_base*      wm_base    = nullptr;  ///< ours, for popups (libdecor binds its own)
        xdg_wm_dialog_v1* wm_dialog  = nullptr;  ///< modal dialogs, if the compositor has it
        libdecor*         decor      = nullptr;
        std::string       app_id;

        // Outputs and their scale (wl_output.scale).
        std::vector<std::pair<wl_output*, int>> outputs;

        // Cursor. Without it libdecor's resize cursor from the window border
        // stays forever once the pointer moves into the content.
        wl_cursor_theme* cursor_theme   = nullptr;
        wl_surface*      cursor_surface = nullptr;
        int              cursor_scale   = 1;
        uint32_t         pointer_enter_serial = 0;

        xkb_context* xkb_ctx    = nullptr;
        xkb_keymap*  keymap     = nullptr;
        xkb_state*   kb_state   = nullptr;

        // The windows with pointer and keyboard focus (ours only: libdecor's
        // decoration surfaces get events on our wl_pointer too).
        WaylandWindow* pointer_window  = nullptr;
        WaylandWindow* keyboard_window = nullptr;
        // Serial of the last button press or key press: xdg_popup.grab and
        // wl_data_device.set_selection need one from real input.
        uint32_t last_input_serial = 0;

        // Clipboard (wl_data_device_manager).
        wl_data_device_manager* data_device_manager = nullptr;
        wl_data_device*         data_device         = nullptr;
        std::string             clipboard_text;
        wl_data_source*         clipboard_source    = nullptr;
        wl_data_offer*          clipboard_offer     = nullptr;
        std::string             clipboard_offer_mime;
        wl_data_offer*          pending_offer       = nullptr;
        std::string             pending_offer_mime;

        Widgets::IbusIme* ime = nullptr;
    };

    Host*                       g = nullptr;
    std::vector<WaylandWindow*> g_wl_windows;

    // --- wl_output ---
    void output_geometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t,
                         int32_t, const char*, const char*, int32_t) {}
    void output_mode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
    void output_done(void*, wl_output*) {}
    void output_scale(void*, wl_output* output, int32_t factor)
    {
        for (auto& entry : g->outputs)
            if (entry.first == output) { entry.second = factor; return; }
        g->outputs.emplace_back(output, factor);
    }
    // name/description (v4) left zero: we bind at most v2.
    const wl_output_listener output_listener = {
        .geometry = output_geometry,
        .mode     = output_mode,
        .done     = output_done,
        .scale    = output_scale,
        .name        = nullptr,
        .description = nullptr,
    };

    int scaleOfOutput(wl_output* output)
    {
        for (const auto& entry : g->outputs)
            if (entry.first == output) return entry.second;
        return 0;
    }

    // --- xdg_wm_base ---
    void wm_base_ping(void*, xdg_wm_base* base, uint32_t serial) { xdg_wm_base_pong(base, serial); }
    const xdg_wm_base_listener wm_base_listener = { wm_base_ping };

    // --- registry ---
    void registry_global(void*, wl_registry* registry, uint32_t name, const char* interface,
                         uint32_t version)
    {
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            g->compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 4));
        } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            g->seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
        } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            g->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
            // wl_output.scale arrived in version 2.
            auto* output = static_cast<wl_output*>(
                wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2u)));
            g->outputs.emplace_back(output, 1);
            wl_output_add_listener(output, &output_listener, nullptr);
        } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
            // Version 1 covers the clipboard; 2+ only add drag-and-drop actions.
            g->data_device_manager = static_cast<wl_data_device_manager*>(
                wl_registry_bind(registry, name, &wl_data_device_manager_interface, 1));
        } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            g->wm_base = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 3u)));
            xdg_wm_base_add_listener(g->wm_base, &wm_base_listener, nullptr);
        } else if (std::strcmp(interface, xdg_wm_dialog_v1_interface.name) == 0) {
            g->wm_dialog = static_cast<xdg_wm_dialog_v1*>(
                wl_registry_bind(registry, name, &xdg_wm_dialog_v1_interface, 1));
        }
    }
    void registry_global_remove(void*, wl_registry*, uint32_t) {}
    const wl_registry_listener registry_listener = { registry_global, registry_global_remove };

    // --- libdecor ---
    void libdecor_error_cb(libdecor*, libdecor_error, const char* msg)
    {
        std::cerr << "[Linux/Wayland] libdecor error: " << msg << "\n";
    }
    libdecor_interface libdecor_iface = { libdecor_error_cb, nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    void frame_configure(libdecor_frame* frame, libdecor_configuration* config, void* data)
    {
        static_cast<WaylandWindow*>(data)->onConfigure(frame, config);
    }
    void frame_close(libdecor_frame*, void* data)
    {
        static_cast<WaylandWindow*>(data)->requestCloseAsync();
    }
    void frame_commit(libdecor_frame*, void* data)
    {
        static_cast<WaylandWindow*>(data)->onFrameCommit();
    }
    void frame_dismiss_popup(libdecor_frame*, const char*, void*) {}
    libdecor_frame_interface libdecor_frame_iface = {
        frame_configure, frame_close, frame_commit, frame_dismiss_popup,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    // --- wl_surface: which output the window is on ---
    void surface_enter(void* data, wl_surface*, wl_output* output)
    {
        if (const int scale = scaleOfOutput(output); scale > 0)
            static_cast<WaylandWindow*>(data)->onSurfaceEnter(scale);
    }
    void surface_leave(void*, wl_surface*, wl_output*) {}
    const wl_surface_listener surface_listener = { surface_enter, surface_leave, nullptr, nullptr };

    // --- xdg_surface / xdg_popup (popups) ---
    void xdg_surface_configure(void* data, xdg_surface*, uint32_t serial)
    {
        static_cast<WaylandWindow*>(data)->onXdgSurfaceConfigure(serial);
    }
    const xdg_surface_listener popup_surface_listener = { xdg_surface_configure };

    void popup_configure(void* data, xdg_popup*, int32_t, int32_t, int32_t width, int32_t height)
    {
        static_cast<WaylandWindow*>(data)->onPopupConfigure(width, height);
    }
    void popup_done(void* data, xdg_popup*)
    {
        // Dismissed by the compositor (a click outside while grabbed).
        static_cast<WaylandWindow*>(data)->requestCloseAsync();
    }
    void popup_repositioned(void*, xdg_popup*, uint32_t) {}
    const xdg_popup_listener popup_listener = { popup_configure, popup_done, popup_repositioned };

    // --- frame callbacks ---
    void frame_done(void* data, wl_callback*, uint32_t)
    {
        static_cast<WaylandWindow*>(data)->onFrameDone();
    }
    const wl_callback_listener frame_listener = { frame_done };

    // --- wl_pointer: routed to the window under the pointer ---
    void pointer_enter(void*, wl_pointer*, uint32_t serial, wl_surface* surface,
                       wl_fixed_t sx, wl_fixed_t sy)
    {
        g->pointer_enter_serial = serial;
        g->pointer_window = WaylandWindow::forSurface(surface);
        if (g->pointer_window)
            g->pointer_window->pointerEnter(static_cast<float>(wl_fixed_to_double(sx)),
                                            static_cast<float>(wl_fixed_to_double(sy)));
    }
    void pointer_leave(void*, wl_pointer*, uint32_t, wl_surface*)
    {
        WaylandWindow* window = g->pointer_window;
        g->pointer_window = nullptr;
        if (window) window->pointerLeave();
    }
    void pointer_motion(void*, wl_pointer*, uint32_t, wl_fixed_t sx, wl_fixed_t sy)
    {
        if (g->pointer_window)
            g->pointer_window->pointerMotion(static_cast<float>(wl_fixed_to_double(sx)),
                                             static_cast<float>(wl_fixed_to_double(sy)));
    }
    void pointer_button(void*, wl_pointer*, uint32_t serial, uint32_t, uint32_t button, uint32_t state)
    {
        if (!g->pointer_window) return;
        const bool pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;
        if (pressed) g->last_input_serial = serial;
        g->pointer_window->pointerButton(button, pressed);
    }
    void pointer_axis(void*, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value)
    {
        if (g->pointer_window)
            g->pointer_window->pointerAxis(axis == WL_POINTER_AXIS_VERTICAL_SCROLL,
                                           static_cast<float>(wl_fixed_to_double(value)));
    }
    void pointer_frame(void*, wl_pointer*) {}
    void pointer_axis_source(void*, wl_pointer*, uint32_t) {}
    void pointer_axis_stop(void*, wl_pointer*, uint32_t, uint32_t) {}
    void pointer_axis_discrete(void*, wl_pointer*, uint32_t, int32_t) {}
    void pointer_axis_value120(void*, wl_pointer*, uint32_t, int32_t) {}
    void pointer_axis_relative_direction(void*, wl_pointer*, uint32_t, uint32_t) {}
    const wl_pointer_listener pointer_listener = {
        pointer_enter, pointer_leave, pointer_motion, pointer_button, pointer_axis,
        pointer_frame, pointer_axis_source, pointer_axis_stop, pointer_axis_discrete,
        pointer_axis_value120, pointer_axis_relative_direction,
    };

    // --- wl_keyboard: routed to the window with keyboard focus ---
    void keyboard_keymap(void*, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
    {
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
        char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (map == MAP_FAILED) { close(fd); return; }
        xkb_keymap* keymap = xkb_keymap_new_from_string(g->xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map, size);
        close(fd);
        if (!keymap) return;
        if (g->kb_state)  xkb_state_unref(g->kb_state);
        if (g->keymap) xkb_keymap_unref(g->keymap);
        g->keymap = keymap;
        g->kb_state  = xkb_state_new(keymap);
    }
    void keyboard_enter(void*, wl_keyboard*, uint32_t, wl_surface* surface, wl_array*)
    {
        g->keyboard_window = WaylandWindow::forSurface(surface);
        if (g->keyboard_window) g->keyboard_window->keyboardEnter();
    }
    void keyboard_leave(void*, wl_keyboard*, uint32_t, wl_surface*)
    {
        WaylandWindow* window = g->keyboard_window;
        g->keyboard_window = nullptr;
        if (window) window->keyboardLeave();
    }
    void keyboard_key(void*, wl_keyboard*, uint32_t serial, uint32_t, uint32_t key, uint32_t state)
    {
        const bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
        if (pressed) g->last_input_serial = serial;
        if (g->keyboard_window) g->keyboard_window->key(key + 8, pressed);  // evdev → xkb
    }
    void keyboard_modifiers(void*, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched,
                            uint32_t locked, uint32_t group)
    {
        if (g->kb_state) xkb_state_update_mask(g->kb_state, depressed, latched, locked, 0, 0, group);
    }
    // Key repeat isn't implemented.
    void keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t) {}
    const wl_keyboard_listener keyboard_listener = {
        keyboard_keymap, keyboard_enter, keyboard_leave, keyboard_key,
        keyboard_modifiers, keyboard_repeat_info,
    };

    // --- clipboard: wl_data_source (we offer the selection) ---
    void data_source_target(void*, wl_data_source*, const char*) {}
    void data_source_send(void*, wl_data_source*, const char*, int32_t fd)
    {
        // Blocking write: clipboard-sized text, not a file transfer.
        const std::string& text = g->clipboard_text;
        size_t written = 0;
        while (written < text.size()) {
            const ssize_t n = write(fd, text.data() + written, text.size() - written);
            if (n <= 0) break;
            written += static_cast<size_t>(n);
        }
        close(fd);
    }
    void data_source_cancelled(void*, wl_data_source* source)
    {
        // Another client took the selection, or setText() replaced it.
        wl_data_source_destroy(source);
        if (g->clipboard_source == source) g->clipboard_source = nullptr;
    }
    const wl_data_source_listener data_source_listener = {
        .target    = data_source_target,
        .send      = data_source_send,
        .cancelled = data_source_cancelled,
        .dnd_drop_performed = nullptr,  // drag-and-drop only
        .dnd_finished       = nullptr,
        .action             = nullptr,
    };

    // --- clipboard: wl_data_offer / wl_data_device (someone else offers it) ---
    void data_offer_offer(void*, wl_data_offer* offer, const char* mime)
    {
        if (g->pending_offer != offer || !g->pending_offer_mime.empty()) return;
        if (std::strcmp(mime, "text/plain;charset=utf-8") == 0 ||
            std::strcmp(mime, "UTF8_STRING") == 0 || std::strcmp(mime, "text/plain") == 0)
            g->pending_offer_mime = mime;
    }
    const wl_data_offer_listener data_offer_listener = { .offer = data_offer_offer, .source_actions = nullptr, .action = nullptr };

    void data_device_data_offer(void*, wl_data_device*, wl_data_offer* offer)
    {
        // Its MIME types (offer events) follow right away.
        g->pending_offer = offer;
        g->pending_offer_mime.clear();
        wl_data_offer_add_listener(offer, &data_offer_listener, nullptr);
    }
    void data_device_selection(void*, wl_data_device*, wl_data_offer* offer)
    {
        if (g->clipboard_offer && g->clipboard_offer != offer) wl_data_offer_destroy(g->clipboard_offer);
        g->clipboard_offer = offer;
        if (offer && offer == g->pending_offer) {
            g->clipboard_offer_mime = g->pending_offer_mime;
            g->pending_offer = nullptr;
            g->pending_offer_mime.clear();
        } else {
            g->clipboard_offer_mime.clear();  // cleared, or an offer we never saw typed
        }
    }
    void data_device_enter(void*, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t,
                           wl_data_offer*) {}
    void data_device_leave(void*, wl_data_device*) {}
    void data_device_motion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {}
    void data_device_drop(void*, wl_data_device*) {}
    const wl_data_device_listener data_device_listener = {
        .data_offer = data_device_data_offer,
        .enter      = data_device_enter,
        .leave      = data_device_leave,
        .motion     = data_device_motion,
        .drop       = data_device_drop,
        .selection  = data_device_selection,
    };

    wl_cursor* loadThemeCursor(const char* css_name, const char* legacy_name)
    {
        wl_cursor* c = wl_cursor_theme_get_cursor(g->cursor_theme, css_name);
        if (!c && legacy_name) c = wl_cursor_theme_get_cursor(g->cursor_theme, legacy_name);
        return c;
    }
} // namespace

namespace systems::leal::campello_widgets::wayland
{
    // ----------------------------------------------------------------------
    // Process level
    // ----------------------------------------------------------------------

    bool init(wl_display* display, std::string app_id)
    {
        g = new Host;
        g->display = display;
        g->app_id  = std::move(app_id);
        g->registry = wl_display_get_registry(display);
        wl_registry_add_listener(g->registry, &registry_listener, nullptr);
        wl_display_roundtrip(display);
        if (!g->compositor) {
            std::cerr << "[Linux/Wayland] wl_compositor not available\n";
            shutdown();
            return false;
        }

        // libdecor creates each window's xdg_surface + xdg_toplevel, uses
        // server-side decorations when the compositor offers them and draws
        // its own otherwise (GNOME).
        g->decor = libdecor_new(display, &libdecor_iface);
        if (!g->decor) {
            std::cerr << "[Linux/Wayland] Failed to create libdecor context\n";
            shutdown();
            return false;
        }

        if (g->seat) {
            g->pointer = wl_seat_get_pointer(g->seat);
            if (g->pointer) wl_pointer_add_listener(g->pointer, &pointer_listener, nullptr);
            g->keyboard = wl_seat_get_keyboard(g->seat);
            if (g->keyboard) {
                g->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
                wl_keyboard_add_listener(g->keyboard, &keyboard_listener, nullptr);
            }
            if (g->data_device_manager) {
                g->data_device = wl_data_device_manager_get_data_device(g->data_device_manager, g->seat);
                if (g->data_device) wl_data_device_add_listener(g->data_device, &data_device_listener, nullptr);
            }
        }
        if (!g->wm_base) std::cerr << "[Linux/Wayland] xdg_wm_base not available; popup windows disabled.\n";

        // The outputs' scale events.
        wl_display_roundtrip(display);
        return true;
    }

    void shutdown()
    {
        if (!g) return;
        if (g->kb_state)  xkb_state_unref(g->kb_state);
        if (g->keymap) xkb_keymap_unref(g->keymap);
        if (g->xkb_ctx)    xkb_context_unref(g->xkb_ctx);
        if (g->cursor_surface) wl_surface_destroy(g->cursor_surface);
        if (g->cursor_theme)   wl_cursor_theme_destroy(g->cursor_theme);
        if (g->keyboard) wl_keyboard_release(g->keyboard);
        if (g->pointer)  wl_pointer_release(g->pointer);
        if (g->clipboard_offer)  wl_data_offer_destroy(g->clipboard_offer);
        if (g->clipboard_source) wl_data_source_destroy(g->clipboard_source);
        if (g->data_device)      wl_data_device_release(g->data_device);
        if (g->data_device_manager) wl_data_device_manager_destroy(g->data_device_manager);
        // wl_seat v5+ needs the release request.
        if (g->seat) {
            if (wl_seat_get_version(g->seat) >= WL_SEAT_RELEASE_SINCE_VERSION) wl_seat_release(g->seat);
            else wl_seat_destroy(g->seat);
        }
        if (g->decor)     libdecor_unref(g->decor);
        if (g->wm_dialog) xdg_wm_dialog_v1_destroy(g->wm_dialog);
        if (g->wm_base)   xdg_wm_base_destroy(g->wm_base);
        for (auto& entry : g->outputs) wl_output_destroy(entry.first);
        if (g->shm)        wl_shm_destroy(g->shm);
        if (g->compositor) wl_compositor_destroy(g->compositor);
        if (g->registry)   wl_registry_destroy(g->registry);
        wl_display_flush(g->display);
        delete g;
        g = nullptr;
    }

    wl_display* display() { return g ? g->display : nullptr; }

    void dispatch()
    {
        if (!g) return;
        wl_display_dispatch_pending(g->display);
        // libdecor's own queue: decoration redraws, resize negotiation.
        libdecor_dispatch(g->decor, 0);
        wl_display_flush(g->display);
    }

    bool dispatchWait(int timeout_ms)
    {
        return g && libdecor_dispatch(g->decor, timeout_ms) >= 0;
    }

    int defaultScale()
    {
        int scale = (g && !g->outputs.empty()) ? g->outputs.front().second : 1;
        return scale < 1 ? 1 : scale;
    }

    void     setIme(IbusIme* ime) { if (g) g->ime = ime; }
    IbusIme* ime()               { return g ? g->ime : nullptr; }

    void initCursor(int scale)
    {
        if (!g || !g->shm || !g->pointer) {
            std::cerr << "[Linux/Wayland] wl_shm or wl_pointer unavailable; cursor shape changes disabled.\n";
            return;
        }
        g->cursor_scale = scale < 1 ? 1 : scale;
        g->cursor_theme = wl_cursor_theme_load(nullptr, 24 * g->cursor_scale, g->shm);
        if (!g->cursor_theme) {
            std::cerr << "[Linux/Wayland] Failed to load cursor theme; cursor shape changes disabled.\n";
            return;
        }
        g->cursor_surface = wl_compositor_create_surface(g->compositor);
        if (g->cursor_surface) wl_surface_set_buffer_scale(g->cursor_surface, g->cursor_scale);
    }

    // ----------------------------------------------------------------------
    // WaylandWindow
    // ----------------------------------------------------------------------

    WaylandWindow::WaylandWindow(View& view, const WindowConfig& config)
        : LinuxWindow(view)
        , kind_(config.kind)
        , owner_view_(config.owner ? &config.owner->view() : nullptr)
        , title_(config.title)
        , modal_(config.modal)
        , resizable_(config.resizable && config.kind != WindowKind::popup)
        , position_(config.position)
    {
        width_  = std::max(1, static_cast<int>(config.size.width  + 0.5f));
        height_ = std::max(1, static_cast<int>(config.size.height + 0.5f));
        scale_  = config.scale > 0 ? config.scale
                : config.owner     ? config.owner->scale_
                                   : defaultScale();
        if (!g || !g->compositor) return;

        surface_ = wl_compositor_create_surface(g->compositor);
        if (!surface_) return;
        wl_surface_add_listener(surface_, &surface_listener, this);
        wl_surface_set_buffer_scale(surface_, scale_);
        g_wl_windows.push_back(this);

        GPU::LinuxSurfaceInfo info{};
        info.display = g->display;
        info.window  = surface_;
        info.api     = GPU::LinuxWindowApi::wayland;
        info.width   = physicalWidth();
        info.height  = physicalHeight();
        attachSurface(info);

        // This window feeds the shared IBus context while one of its text
        // fields has the input target and the window has keyboard focus.
        view.textInputManager().setOnInputTargetChanged([this](bool has_target) {
            IbusIme* ime = g ? g->ime : nullptr;
            if (!ime || !ime->isActive() || !keyboard_focus_) return;
            if (has_target) { ime->focusIn(); updateImeCursorPosition(); }
            else            ime->focusOut();
        });
    }

    WaylandWindow::~WaylandWindow()
    {
        if (View::isAlive(&view())) view().textInputManager().setOnInputTargetChanged(nullptr);
        if (g) {
            if (g->pointer_window == this)  g->pointer_window = nullptr;
            if (g->keyboard_window == this) {
                g->keyboard_window = nullptr;
                if (g->ime && g->ime->isActive()) g->ime->focusOut();
            }
        }
        hideOwnedPopups();
        // The swapchain before the wl_surface it presents to.
        releaseSurface();
        destroyRole();
        g_wl_windows.erase(std::remove(g_wl_windows.begin(), g_wl_windows.end(), this),
                           g_wl_windows.end());
        if (surface_) wl_surface_destroy(surface_);
        surface_ = nullptr;
        if (g) wl_display_flush(g->display);
    }

    uint32_t WaylandWindow::physicalWidth() const
    {
        return static_cast<uint32_t>(std::max(0, width_ * scale_));
    }
    uint32_t WaylandWindow::physicalHeight() const
    {
        return static_cast<uint32_t>(std::max(0, height_ * scale_));
    }

    WaylandWindow* WaylandWindow::owner() const
    {
        if (!owner_view_) return nullptr;
        for (WaylandWindow* w : g_wl_windows)
            if (&w->view() == owner_view_) return w;
        return nullptr;
    }

    WaylandWindow* WaylandWindow::forSurface(const wl_surface* surface)
    {
        if (!surface) return nullptr;
        for (WaylandWindow* w : g_wl_windows)
            if (w->surface_ == surface) return w;
        return nullptr;
    }

    WaylandWindow* WaylandWindow::forView(const View* view)
    {
        for (WaylandWindow* w : g_wl_windows)
            if (&w->view() == view) return w;
        return nullptr;
    }

    bool WaylandWindow::anyOtherShown(const WaylandWindow* except)
    {
        for (WaylandWindow* w : g_wl_windows)
            if (w != except && w->kind_ != WindowKind::popup && w->mapped_) return true;
        return false;
    }

    void WaylandWindow::show(bool /*activate*/)
    {
        // Wayland clients can't raise or focus their own windows (that
        // needs xdg_activation); the compositor usually focuses a new one.
        if (!surface_ || mapped_) return;
        // Dialogs and popups are attached to their owner's xdg_surface: wait
        // until the owner has one and was configured (a window opened as the
        // app starts).
        if (WaylandWindow* owner = this->owner(); owner && !owner->ever_configured_) {
            pending_show_ = true;
            return;
        }
        pending_show_ = false;
        if (kind_ == WindowKind::popup) createPopup();
        else                            createFrame();
        if (!frame_ && !popup_) return;
        mapped_ = true;
        if (paused_) { paused_ = false; resumeRendering(); }
        wl_display_flush(g->display);
    }

    void WaylandWindow::createFrame()
    {
        frame_ = libdecor_decorate(g->decor, surface_, &libdecor_frame_iface, this);
        if (!frame_) {
            std::cerr << "[Linux/Wayland] Failed to decorate a window\n";
            return;
        }
        libdecor_frame_set_title(frame_, title_.c_str());
        libdecor_frame_set_app_id(frame_, g->app_id.c_str());

        const bool dialog = kind_ == WindowKind::dialog || modal_;
        int unset = 0;
        if (!resizable_) unset |= LIBDECOR_ACTION_RESIZE;
        if (dialog)      unset |= LIBDECOR_ACTION_MINIMIZE | LIBDECOR_ACTION_FULLSCREEN;
        if (unset) libdecor_frame_unset_capabilities(frame_, static_cast<libdecor_capabilities>(unset));
        if (!resizable_) {
            libdecor_frame_set_min_content_size(frame_, width_, height_);
            libdecor_frame_set_max_content_size(frame_, width_, height_);
        }

        // Owned windows stay with their owner (the compositor stacks them
        // above it and usually centres dialogs on it).
        WaylandWindow* parent = owner();
        while (parent && !parent->frame_) parent = parent->owner();  // a popup's owner
        if (parent) libdecor_frame_set_parent(frame_, parent->frame_);

        libdecor_frame_map(frame_);

        if (modal_ && g->wm_dialog) {
            if (xdg_toplevel* toplevel = libdecor_frame_get_xdg_toplevel(frame_)) {
                dialog_ = xdg_wm_dialog_v1_get_xdg_dialog(g->wm_dialog, toplevel);
                xdg_dialog_v1_set_modal(dialog_);
            }
        }
    }

    void WaylandWindow::createPopup()
    {
        WaylandWindow* owner = this->owner();
        xdg_surface* parent = nullptr;
        if (owner) parent = owner->frame_ ? libdecor_frame_get_xdg_surface(owner->frame_) : owner->xdg_surface_;
        if (!g->wm_base || !parent) {
            std::cerr << "[Linux/Wayland] A popup window needs a shown owner window\n";
            return;
        }

        // `position` is relative to the owner's content; xdg positions are
        // relative to its window geometry, which includes libdecor's title
        // bar when it draws one.
        const Offset at = position_.value_or(Offset{0.0f, 0.0f});
        int x = static_cast<int>(at.x + 0.5f), y = static_cast<int>(at.y + 0.5f);
        if (owner->frame_) libdecor_frame_translate_coordinate(owner->frame_, x, y, &x, &y);

        xdg_positioner* positioner = xdg_wm_base_create_positioner(g->wm_base);
        xdg_positioner_set_size(positioner, width_, height_);
        xdg_positioner_set_anchor_rect(positioner, x, y, 1, 1);
        xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
        xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
        // Kept on screen by the compositor: flipped or slid at an edge.
        xdg_positioner_set_constraint_adjustment(positioner,
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y);

        xdg_surface_ = xdg_wm_base_get_xdg_surface(g->wm_base, surface_);
        xdg_surface_add_listener(xdg_surface_, &popup_surface_listener, this);
        popup_ = xdg_surface_get_popup(xdg_surface_, parent, positioner);
        xdg_popup_add_listener(popup_, &popup_listener, this);
        xdg_positioner_destroy(positioner);

        // A grab gives the menu keyboard focus, and the compositor dismisses
        // it (popup_done) on a click elsewhere. It needs the serial of the
        // input that opened it; without one (no input yet) the popup is
        // shown ungrabbed.
        if (g->seat && g->last_input_serial) xdg_popup_grab(popup_, g->seat, g->last_input_serial);
        wl_surface_commit(surface_);  // initial commit: no buffer, asks for a configure
    }

    void WaylandWindow::hideOwnedPopups()
    {
        // A parent's xdg_surface must outlive its popups.
        const std::vector<WaylandWindow*> windows = g_wl_windows;
        for (WaylandWindow* w : windows)
            if (w != this && w->kind_ == WindowKind::popup && w->owner_view_ == &view()) w->hide();
    }

    void WaylandWindow::destroyRole()
    {
        if (frame_callback_) { wl_callback_destroy(frame_callback_); frame_callback_ = nullptr; }
        if (dialog_)      { xdg_dialog_v1_destroy(dialog_); dialog_ = nullptr; }
        if (popup_)       { xdg_popup_destroy(popup_); popup_ = nullptr; }
        if (xdg_surface_) { xdg_surface_destroy(xdg_surface_); xdg_surface_ = nullptr; }
        if (frame_)       { libdecor_frame_unref(frame_); frame_ = nullptr; }
    }

    void WaylandWindow::hide()
    {
        if (pending_show_) { pending_show_ = false; return; }
        if (!mapped_ || !surface_) return;
        hideOwnedPopups();
        // No present may race the null buffer below.
        pauseRendering();
        paused_ = true;
        destroyRole();
        wl_surface_attach(surface_, nullptr, 0, 0);
        wl_surface_commit(surface_);
        mapped_     = false;
        configured_ = false;
        if (g) {
            if (g->pointer_window == this) g->pointer_window = nullptr;
            if (g->keyboard_window == this) {
                keyboardLeave();
                g->keyboard_window = nullptr;
            }
            wl_display_flush(g->display);
        }
    }

    void WaylandWindow::setTitle(const std::string& title)
    {
        title_ = title;
        if (frame_) libdecor_frame_set_title(frame_, title_.c_str());
    }

    void WaylandWindow::setLogicalSize(Size size)
    {
        if (kind_ == WindowKind::popup) return;
        const int w = std::max(1, static_cast<int>(size.width  + 0.5f));
        const int h = std::max(1, static_cast<int>(size.height + 0.5f));
        if (frame_ && !resizable_) {
            libdecor_frame_set_min_content_size(frame_, w, h);
            libdecor_frame_set_max_content_size(frame_, w, h);
        }
        if (frame_ && configured_) {
            libdecor_state* state = libdecor_state_new(w, h);
            libdecor_frame_commit(frame_, state, nullptr);
            libdecor_state_free(state);
        }
        applySize(w, h);  // may destroy `this`
    }

    void WaylandWindow::setScale(int scale)
    {
        if (scale < 1 || scale == scale_ || !surface_) return;
        scale_ = scale;
        wl_surface_set_buffer_scale(surface_, scale_);
        resizeSurface(physicalWidth(), physicalHeight());
        notifyMetricsChanged();
    }

    void WaylandWindow::applySize(int w, int h)
    {
        if (w == width_ && h == height_) return;
        width_  = w;
        height_ = h;
        // Wayland reports currentExtent = UINT32_MAX: the swapchain can't see
        // the new size by itself (rebuilt at its next acquire).
        resizeSurface(physicalWidth(), physicalHeight());
        notifyMetricsChanged();  // MediaQuery, full repaint; may destroy `this`
    }

    void WaylandWindow::showPendingOwnedWindows()
    {
        const std::vector<WaylandWindow*> windows = g_wl_windows;
        for (WaylandWindow* w : windows)
            if (isLive(w) && w->pending_show_ && w->owner_view_ == &view()) w->show();
    }

    void WaylandWindow::onConfigure(libdecor_frame* frame, libdecor_configuration* configuration)
    {
        int w = width_, h = height_;
        if (!libdecor_configuration_get_content_size(configuration, frame, &w, &h) || w <= 0 || h <= 0) {
            w = width_;
            h = height_;
        }
        libdecor_state* state = libdecor_state_new(w, h);
        libdecor_frame_commit(frame, state, configuration);
        libdecor_state_free(state);

        configured_ = true;
        const bool first = !ever_configured_;
        ever_configured_ = true;
        if (renderer()) renderer()->notePaintRequested();
        requestFrame();
        if (first) showPendingOwnedWindows();
        if (isLive(this)) applySize(w, h);  // may destroy `this`
    }

    void WaylandWindow::onFrameCommit()
    {
        // libdecor wants the surface committed to finish a configure; repaint
        // too, as some compositors blank a surface committed without a buffer.
        if (renderer()) renderer()->notePaintRequested();
        requestFrame();
        wl_surface_commit(surface_);
    }

    void WaylandWindow::onXdgSurfaceConfigure(uint32_t serial)
    {
        if (xdg_surface_) xdg_surface_ack_configure(xdg_surface_, serial);
        configured_ = true;
        const bool first = !ever_configured_;
        ever_configured_ = true;
        if (renderer()) renderer()->notePaintRequested();
        requestFrame();
        if (first) showPendingOwnedWindows();
    }

    void WaylandWindow::onPopupConfigure(int width, int height)
    {
        // Applied with the xdg_surface.configure that follows.
        if (width > 0 && height > 0) applySize(width, height);
    }

    void WaylandWindow::onFrameDone()
    {
        if (frame_callback_) wl_callback_destroy(frame_callback_);
        frame_callback_ = nullptr;
        // A frame requested meanwhile can go now.
        if (needsFrame()) linux_host::wake();
    }

    void WaylandWindow::onSurfaceEnter(int output_scale)
    {
        // The window's scale is fixed once it has drawn (changing the buffer
        // scale races the raster thread's presents); before that, follow the
        // output it appeared on (see runAppWayland()).
        if (!renderer()) setScale(output_scale);
    }

    void WaylandWindow::requestCloseAsync()
    {
        // Delivered on a later turn, like every platform's close request:
        // the handler may destroy this window.
        WaylandWindow* self = this;
        linux_host::postTask([self] {
            if (!isLive(self)) return;
            auto callback = self->on_close;
            if (callback) callback();
        });
    }

    bool WaylandWindow::frameThrottled() const
    {
        // Waiting for the compositor's go-ahead for the last frame. A
        // callback that never comes (the frame was dropped, the window is
        // hidden by the compositor) stops counting after 100 ms.
        return frame_callback_ &&
               std::chrono::steady_clock::now() - frame_requested_at_ < std::chrono::milliseconds(100);
    }

    void WaylandWindow::willSubmitFrame()
    {
        // Requested before the raster thread presents (commits) the frame,
        // so the request applies to that commit.
        if (frame_callback_) wl_callback_destroy(frame_callback_);
        frame_callback_ = wl_surface_frame(surface_);
        wl_callback_add_listener(frame_callback_, &frame_listener, this);
        frame_requested_at_ = std::chrono::steady_clock::now();
    }

    // ----------------------------------------------------------------------
    // Input
    // ----------------------------------------------------------------------

    void WaylandWindow::setCursor(SystemMouseCursor cursor)
    {
        if (!g || g->pointer_window != this || !g->cursor_theme || !g->cursor_surface) return;
        const char* css_name    = "default";
        const char* legacy_name = "left_ptr";
        switch (cursor) {
            case SystemMouseCursor::pointer:   css_name = "pointer";     legacy_name = "hand2"; break;
            case SystemMouseCursor::text:      css_name = "text";        legacy_name = "xterm"; break;
            case SystemMouseCursor::forbidden: css_name = "not-allowed"; legacy_name = "crossed_circle"; break;
            case SystemMouseCursor::resize_ns: css_name = "ns-resize";   legacy_name = "sb_v_double_arrow"; break;
            case SystemMouseCursor::resize_ew: css_name = "ew-resize";   legacy_name = "sb_h_double_arrow"; break;
            default: break;
        }
        wl_cursor* wl_cur = loadThemeCursor(css_name, legacy_name);
        if (!wl_cur || wl_cur->image_count == 0) return;
        wl_cursor_image* image  = wl_cur->images[0];
        wl_buffer*       buffer = wl_cursor_image_get_buffer(image);
        if (!buffer) return;
        wl_surface_attach(g->cursor_surface, buffer, 0, 0);
        wl_surface_damage(g->cursor_surface, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_commit(g->cursor_surface);
        // The hotspot is in surface (logical) coordinates.
        wl_pointer_set_cursor(g->pointer, g->pointer_enter_serial, g->cursor_surface,
                              static_cast<int32_t>(image->hotspot_x) / g->cursor_scale,
                              static_cast<int32_t>(image->hotspot_y) / g->cursor_scale);
    }

    void WaylandWindow::updateImeCursorPosition()
    {
        IbusIme* ime = g ? g->ime : nullptr;
        if (!ime || !ime->isActive()) return;
        TextInputManager& tim = view().textInputManager();
        if (!tim.hasInputTarget() || !tim.activeController()) return;
        auto rect = tim.getCharacterRect(tim.activeController()->selectionEnd());
        if (rect[2] <= 0.0f || rect[3] <= 0.0f) return;
        // Wayland has no global coordinates: surface-local.
        ime->setCursorLocation(static_cast<int>(rect[0]), static_cast<int>(rect[1] + rect[3]),
                               static_cast<int>(rect[2]), static_cast<int>(rect[3]));
    }

    void WaylandWindow::pointerEnter(float x, float y)
    {
        // Reclaim the cursor at once: libdecor's border cursor would stay
        // until the pointer crossed a MouseRegion.
        setCursor(SystemMouseCursor::arrow);
        pointerMotion(x, y);
    }

    void WaylandWindow::pointerLeave()
    {
        // A move far outside ends the window's hovers. Not during a drag:
        // the compositor keeps sending it to this surface.
        if (mouse_pressed_) return;
        View::Scope scope(&view());
        PointerEvent e;
        e.kind        = PointerEventKind::move;
        e.pointer_id  = 0;
        e.position    = { -100000.0f, -100000.0f };
        e.pressure    = 0.0f;
        e.device_kind = PointerDeviceKind::mouse;
        view().pointerDispatcher().handlePointerEvent(e);
    }

    void WaylandWindow::pointerMotion(float x, float y)
    {
        pointer_x_ = x;
        pointer_y_ = y;
        View::Scope scope(&view());
        PointerEvent e;
        e.kind        = PointerEventKind::move;
        e.pointer_id  = 0;
        e.position    = { x, y };
        e.pressure    = mouse_pressed_ ? 1.0f : 0.0f;
        e.device_kind = PointerDeviceKind::mouse;
        view().pointerDispatcher().handlePointerEvent(e);
    }

    void WaylandWindow::pointerButton(uint32_t button, bool pressed)
    {
        View::Scope scope(&view());
        PointerEvent e;
        e.kind        = pressed ? PointerEventKind::down : PointerEventKind::up;
        e.pointer_id  = 0;
        e.position    = { pointer_x_, pointer_y_ };
        e.pressure    = pressed ? 1.0f : 0.0f;
        e.device_kind = PointerDeviceKind::mouse;
        // Linux evdev codes: BTN_LEFT, BTN_RIGHT, BTN_MIDDLE.
        switch (button) {
            case 0x110: e.button = PointerButton::primary;   break;
            case 0x111: e.button = PointerButton::secondary; break;
            case 0x112: e.button = PointerButton::tertiary;  break;
            default:    e.button = PointerButton::unknown;   break;
        }
        mouse_pressed_ = pressed;
        // A modal window over this one blocks it (the core drops the input);
        // the compositor keeps a modal dialog (xdg_dialog_v1) in front.
        view().pointerDispatcher().handlePointerEvent(e);
        if (pressed && isLive(this)) updateImeCursorPosition();
    }

    void WaylandWindow::pointerAxis(bool vertical, float delta)
    {
        View::Scope scope(&view());
        PointerEvent e;
        e.kind        = PointerEventKind::scroll;
        e.pointer_id  = 0;
        e.position    = { pointer_x_, pointer_y_ };
        e.pressure    = 0.0f;
        e.device_kind = PointerDeviceKind::mouse;
        if (vertical) e.scroll_delta_y = delta;
        else          e.scroll_delta_x = delta;
        view().pointerDispatcher().handlePointerEvent(e);
    }

    void WaylandWindow::keyboardEnter()
    {
        keyboard_focus_ = true;
        IbusIme* ime = g ? g->ime : nullptr;
        if (ime && ime->isActive() && view().textInputManager().hasInputTarget()) {
            ime->focusIn();
            updateImeCursorPosition();
        }
    }

    void WaylandWindow::keyboardLeave()
    {
        keyboard_focus_ = false;
        IbusIme* ime = g ? g->ime : nullptr;
        if (ime && ime->isActive() && view().textInputManager().hasInputTarget()) ime->focusOut();
    }

    void WaylandWindow::key(uint32_t keycode, bool pressed)
    {
        if (!g || !g->kb_state) return;
        View::Scope scope(&view());
        FocusManager& focus = view().focusManager();
        const xkb_keysym_t keysym = xkb_state_key_get_one_sym(g->kb_state, keycode);

        KeyEvent ke;
        ke.key_code  = keysymToKeyCode(keysym);
        ke.modifiers = xkbStateToKeyModifiers(g->kb_state);
        ke.character = 0;
        if (!pressed) {
            ke.kind = KeyEventKind::up;
            focus.handleKeyEvent(ke);
            return;
        }
        ke.kind = KeyEventKind::down;

        // Navigation / special keys bypass the IME.
        if (isNavigationOrSpecialKey(ke.key_code)) {
            focus.handleKeyEvent(ke);
            return;
        }
        // IBus first while a text field has the input target, except Ctrl
        // shortcuts (never composition input; see FocusManager::setGlobalKeyHandler()).
        TextInputManager& tim = view().textInputManager();
        if (g->ime && g->ime->isActive() && tim.hasInputTarget() && !(ke.modifiers & KeyModifiers::ctrl)) {
            if (g->ime->processKeyEvent(static_cast<uint32_t>(keysym), keycode, ke.modifiers))
                return;  // consumed for composition
        }
        ke.character = xkb_state_key_get_utf32(g->kb_state, keycode);
        focus.handleKeyEvent(ke);
    }

} // namespace systems::leal::campello_widgets::wayland

// ============================================================================
// Clipboard bridge (called from clipboard.cpp when isRunningUnderWayland())
// ============================================================================

namespace systems::leal::campello_widgets
{
    void setWaylandClipboardText(const std::string& text)
    {
        if (!g || !g->data_device_manager || !g->data_device) return;
        g->clipboard_text = text;
        if (g->clipboard_source) {
            wl_data_source_destroy(g->clipboard_source);
            g->clipboard_source = nullptr;
        }
        wl_data_source* source = wl_data_device_manager_create_data_source(g->data_device_manager);
        if (!source) return;
        wl_data_source_offer(source, "text/plain;charset=utf-8");
        wl_data_source_offer(source, "UTF8_STRING");
        wl_data_source_offer(source, "text/plain");
        wl_data_source_add_listener(source, &data_source_listener, nullptr);
        // Needs a serial from real input (a strict compositor ignores the
        // request before the first key or click).
        wl_data_device_set_selection(g->data_device, source, g->last_input_serial);
        g->clipboard_source = source;
        wl_display_flush(g->display);
    }

    std::string getWaylandClipboardText()
    {
        if (!g) return {};
        // Our own selection: compositors don't reliably send it back to us.
        if (g->clipboard_source) return g->clipboard_text;
        if (!g->clipboard_offer || g->clipboard_offer_mime.empty()) return {};

        int fds[2];
        if (pipe(fds) != 0) return {};
        wl_data_offer_receive(g->clipboard_offer, g->clipboard_offer_mime.c_str(), fds[1]);
        close(fds[1]);
        wl_display_flush(g->display);

        // A deadline guards against a source that never writes or closes.
        std::string result;
        char buf[4096];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        for (;;) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) break;
            pollfd pfd{fds[0], POLLIN, 0};
            const int timeout_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            if (poll(&pfd, 1, timeout_ms) <= 0) break;
            const ssize_t n = read(fds[0], buf, sizeof buf);
            if (n <= 0) break;
            result.append(buf, static_cast<size_t>(n));
        }
        close(fds[0]);
        return result;
    }
} // namespace systems::leal::campello_widgets

#endif // CAMPELLO_WIDGETS_HAS_WAYLAND
