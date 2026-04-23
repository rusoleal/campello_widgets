#ifdef CAMPHELLO_WIDGETS_HAS_WAYLAND

// -----------------------------------------------------------------------------
// Wayland platform runner
// -----------------------------------------------------------------------------
// Creates a Wayland window backed by campello_gpu's Vulkan backend.
// Supports pointer, keyboard (via xkbcommon), and IBus IME.
//
// Requires: wayland-client >= 1.20, xkbcommon
// -----------------------------------------------------------------------------

#include <campello_widgets/linux/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/platform/linux_surface.hpp>

#include "ibus_ime.hpp"
#include "vulkan_draw_backend.hpp"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <climits>
#include <cstdlib>

#include "protocols/xdg-shell-client-protocol.h"

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ============================================================================
// Shared helpers (duplicated from run_app.cpp to avoid cross-file coupling)
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

static uint32_t xkbStateToKeyModifiers(struct xkb_state* state, xkb_keycode_t keycode)
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,  XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::shift;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,   XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::ctrl;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,    XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::alt;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,   XKB_STATE_MODS_EFFECTIVE) > 0)
        mods |= Widgets::KeyModifiers::meta;
    return mods;
}

static bool isNavigationOrSpecialKey(Widgets::KeyCode key_code)
{
    return key_code == Widgets::KeyCode::left
        || key_code == Widgets::KeyCode::right
        || key_code == Widgets::KeyCode::up
        || key_code == Widgets::KeyCode::down
        || key_code == Widgets::KeyCode::home
        || key_code == Widgets::KeyCode::end
        || key_code == Widgets::KeyCode::page_up
        || key_code == Widgets::KeyCode::page_down
        || key_code == Widgets::KeyCode::escape
        || key_code == Widgets::KeyCode::tab
        || key_code == Widgets::KeyCode::enter
        || key_code == Widgets::KeyCode::backspace
        || key_code == Widgets::KeyCode::delete_forward
        || key_code == Widgets::KeyCode::f1
        || key_code == Widgets::KeyCode::f2
        || key_code == Widgets::KeyCode::f3
        || key_code == Widgets::KeyCode::f4
        || key_code == Widgets::KeyCode::f5
        || key_code == Widgets::KeyCode::f6
        || key_code == Widgets::KeyCode::f7
        || key_code == Widgets::KeyCode::f8
        || key_code == Widgets::KeyCode::f9
        || key_code == Widgets::KeyCode::f10
        || key_code == Widgets::KeyCode::f11
        || key_code == Widgets::KeyCode::f12;
}

// ============================================================================
// Wayland window state
// ============================================================================

struct WaylandWindowState
{
    struct wl_display*    display      = nullptr;
    struct wl_registry*   registry     = nullptr;
    struct wl_compositor* compositor   = nullptr;
    struct wl_surface*    surface      = nullptr;
    struct wl_seat*       seat         = nullptr;
    struct wl_pointer*    pointer      = nullptr;
    struct wl_keyboard*   keyboard     = nullptr;
    struct xdg_wm_base*   xdg_wm_base  = nullptr;
    struct xdg_surface*   xdg_surface  = nullptr;
    struct xdg_toplevel*  xdg_toplevel = nullptr;

    struct xkb_context*   xkb_ctx      = nullptr;
    struct xkb_keymap*    xkb_keymap   = nullptr;
    struct xkb_state*     xkb_state    = nullptr;

    std::shared_ptr<GPU::Device>                device;
    std::shared_ptr<Widgets::Renderer>          renderer;
    std::shared_ptr<Widgets::Element>           root_element;
    std::shared_ptr<Widgets::PointerDispatcher> dispatcher;
    std::shared_ptr<Widgets::FocusManager>      focus_manager;
    std::unique_ptr<Widgets::TickerScheduler>   ticker_scheduler;
    std::unique_ptr<Widgets::TextInputManager>  text_input_manager;
    std::unique_ptr<Widgets::IbusIme>           ibus_ime;

    bool running      = true;
    bool needs_redraw = true;
    bool configured   = false;
    bool closed       = false;
    int  width        = 800;
    int  height       = 600;
    bool mouse_pressed = false;
    float last_pointer_x = 0.0f;
    float last_pointer_y = 0.0f;
};

// ============================================================================
// Wayland listeners
// ============================================================================

// --- Registry ---
static void registry_global(void* data, struct wl_registry* registry,
                            uint32_t name, const char* interface, uint32_t version)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = static_cast<struct wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = static_cast<struct wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface, 7));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = static_cast<struct xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove
};

// --- xdg_wm_base ---
static void xdg_wm_base_ping(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener_impl = {
    xdg_wm_base_ping
};

// --- xdg_surface ---
static void xdg_surface_configure(void* data, struct xdg_surface* xdg_surface, uint32_t serial)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    state->configured = true;
    state->needs_redraw = true;
}

static const struct xdg_surface_listener xdg_surface_listener_impl = {
    xdg_surface_configure
};

// --- xdg_toplevel ---
static void xdg_toplevel_configure(void* data, struct xdg_toplevel* toplevel,
                                   int32_t width, int32_t height, struct wl_array* states)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (width > 0)  state->width  = width;
    if (height > 0) state->height = height;
    state->needs_redraw = true;
}

static void xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    state->closed = true;
    state->running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener_impl = {
    xdg_toplevel_configure,
    xdg_toplevel_close
};

// --- wl_pointer ---
static void pointer_enter(void* data, struct wl_pointer* pointer,
                          uint32_t serial, struct wl_surface* surface,
                          wl_fixed_t sx, wl_fixed_t sy)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    state->last_pointer_x = static_cast<float>(wl_fixed_to_double(sx));
    state->last_pointer_y = static_cast<float>(wl_fixed_to_double(sy));
}

static void pointer_leave(void* data, struct wl_pointer* pointer,
                          uint32_t serial, struct wl_surface* surface) {}

static void pointer_motion(void* data, struct wl_pointer* pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (!state->dispatcher) return;
    state->last_pointer_x = static_cast<float>(wl_fixed_to_double(sx));
    state->last_pointer_y = static_cast<float>(wl_fixed_to_double(sy));

    Widgets::PointerEvent e;
    e.kind       = Widgets::PointerEventKind::move;
    e.pointer_id = 0;
    e.position   = { state->last_pointer_x, state->last_pointer_y };
    e.pressure   = state->mouse_pressed ? 1.0f : 0.0f;
    state->dispatcher->handlePointerEvent(e);
}

static void pointer_button(void* data, struct wl_pointer* pointer,
                           uint32_t serial, uint32_t time,
                           uint32_t button, uint32_t state_val)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (!state->dispatcher) return;

    Widgets::PointerEvent e;
    e.pointer_id = 0;
    e.position   = { state->last_pointer_x, state->last_pointer_y };

    if (state_val == WL_POINTER_BUTTON_STATE_PRESSED) {
        e.kind = Widgets::PointerEventKind::down;
        e.pressure = 1.0f;
        state->mouse_pressed = true;
    } else {
        e.kind = Widgets::PointerEventKind::up;
        e.pressure = 0.0f;
        state->mouse_pressed = false;
    }
    state->dispatcher->handlePointerEvent(e);
}

static void pointer_axis(void* data, struct wl_pointer* pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value) {}

static const struct wl_pointer_listener pointer_listener = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis
};

// --- wl_keyboard ---
static void keyboard_keymap(void* data, struct wl_keyboard* keyboard,
                            uint32_t format, int32_t fd, uint32_t size)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    char* map_str = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }
    state->xkb_keymap = xkb_keymap_new_from_string(
        state->xkb_ctx, map_str,
        XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map_str, size);
    close(fd);
    if (state->xkb_keymap) {
        state->xkb_state = xkb_state_new(state->xkb_keymap);
    }
}

static void keyboard_enter(void* data, struct wl_keyboard* keyboard,
                           uint32_t serial, struct wl_surface* surface,
                           struct wl_array* keys) {}

static void keyboard_leave(void* data, struct wl_keyboard* keyboard,
                           uint32_t serial, struct wl_surface* surface) {}

static void keyboard_key(void* data, struct wl_keyboard* keyboard,
                         uint32_t serial, uint32_t time,
                         uint32_t key, uint32_t state_val)
{
    auto* ws = static_cast<WaylandWindowState*>(data);
    if (!ws->focus_manager) return;

    xkb_keycode_t keycode = key + 8; // Wayland keycodes are offset by 8
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(ws->xkb_state, keycode);
    Widgets::KeyCode key_code = keysymToKeyCode(keysym);
    uint32_t mods = xkbStateToKeyModifiers(ws->xkb_state, keycode);

    if (state_val == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        // Navigation / special keys bypass IME
        if (isNavigationOrSpecialKey(key_code)) {
            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::down;
            ke.key_code  = key_code;
            ke.modifiers = mods;
            ke.character = 0;
            ws->focus_manager->handleKeyEvent(ke);
            return;
        }

        bool consumed_by_ime = false;
        if (ws->ibus_ime && ws->ibus_ime->isActive() &&
            ws->text_input_manager && ws->text_input_manager->hasInputTarget())
        {
            consumed_by_ime = ws->ibus_ime->processKeyEvent(
                static_cast<uint32_t>(keysym),
                static_cast<uint32_t>(keycode),
                mods);
        }

        if (!consumed_by_ime) {
            uint32_t character = xkb_state_key_get_utf32(ws->xkb_state, keycode);

            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::down;
            ke.key_code  = key_code;
            ke.modifiers = mods;
            ke.character = character;
            ws->focus_manager->handleKeyEvent(ke);
        }
    }
    else // released
    {
        Widgets::KeyEvent ke;
        ke.kind      = Widgets::KeyEventKind::up;
        ke.key_code  = key_code;
        ke.modifiers = mods;
        ke.character = 0;
        ws->focus_manager->handleKeyEvent(ke);
    }
}

static void keyboard_modifiers(void* data, struct wl_keyboard* keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (state->xkb_state) {
        xkb_state_update_mask(state->xkb_state,
            mods_depressed, mods_latched, mods_locked,
            0, 0, group);
    }
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers
};

// --- Frame callback ---
static void frame_done(void* data, struct wl_callback* callback, uint32_t time)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    state->needs_redraw = true;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener frame_listener = {
    frame_done
};

// ============================================================================
// Render frame helper
// ============================================================================

static void renderFrame(WaylandWindowState* state)
{
    if (!state || !state->renderer || !state->device) return;

    auto color_view = state->device->getSwapchainTextureView();
    if (!color_view) return;

    auto now = std::chrono::steady_clock::now();
    uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
    if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);

    if (auto* backend = state->renderer->drawBackend()) {
        backend->setViewport(static_cast<float>(state->width),
                             static_cast<float>(state->height));
    }

    state->renderer->renderFrame(
        color_view,
        static_cast<float>(state->width),
        static_cast<float>(state->height));
}

// ============================================================================
// IME cursor position helper
// ============================================================================

static void updateImeCursorPosition(WaylandWindowState* state)
{
    if (!state || !state->ibus_ime || !state->ibus_ime->isActive()) return;
    if (!state->text_input_manager || !state->text_input_manager->hasInputTarget()) return;

    auto rect = state->text_input_manager->getCharacterRect(
        state->text_input_manager->activeController()->selectionEnd());

    if (rect[2] <= 0.0f || rect[3] <= 0.0f) return;

    // Wayland doesn't have a global coordinate system like X11;
    // use surface-local coordinates for the cursor rect.
    state->ibus_ime->setCursorLocation(
        static_cast<int>(rect[0]),
        static_cast<int>(rect[1] + rect[3]),
        static_cast<int>(rect[2]),
        static_cast<int>(rect[3]));
}

// ============================================================================
// Main Wayland runApp implementation
// ============================================================================

namespace systems::leal::campello_widgets
{

int runAppWayland(const std::string& title, int width, int height,
                  WidgetRef root_widget, bool resizable)
{
    (void)resizable; // Wayland compositor handles resizing

    // -------------------------------------------------------------------------
    // Connect to Wayland display
    // -------------------------------------------------------------------------
    struct wl_display* display = wl_display_connect(nullptr);
    if (!display) {
        std::cerr << "[Linux/Wayland] Failed to connect to Wayland display\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // Get registry and roundtrip to discover globals
    // -------------------------------------------------------------------------
    WaylandWindowState state;
    state.display = display;
    state.width   = width;
    state.height  = height;

    state.registry = wl_display_get_registry(display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(display);

    if (!state.compositor) {
        std::cerr << "[Linux/Wayland] wl_compositor not available\n";
        wl_display_disconnect(display);
        return 1;
    }
    if (!state.xdg_wm_base) {
        std::cerr << "[Linux/Wayland] xdg_wm_base not available\n";
        wl_display_disconnect(display);
        return 1;
    }

    xdg_wm_base_add_listener(state.xdg_wm_base, &xdg_wm_base_listener_impl, &state);

    // -------------------------------------------------------------------------
    // Create surface and XDG toplevel
    // -------------------------------------------------------------------------
    state.surface = wl_compositor_create_surface(state.compositor);
    if (!state.surface) {
        std::cerr << "[Linux/Wayland] Failed to create wl_surface\n";
        wl_display_disconnect(display);
        return 1;
    }

    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener_impl, &state);

    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener_impl, &state);
    xdg_toplevel_set_title(state.xdg_toplevel, title.c_str());

    wl_surface_commit(state.surface);
    wl_display_roundtrip(display);

    // Wait for the first configure before rendering
    while (!state.configured && state.running) {
        wl_display_dispatch(display);
    }
    if (!state.running) {
        wl_display_disconnect(display);
        return 0;
    }

    // -------------------------------------------------------------------------
    // Set up input devices
    // -------------------------------------------------------------------------
    if (state.seat) {
        state.pointer = wl_seat_get_pointer(state.seat);
        if (state.pointer) {
            wl_pointer_add_listener(state.pointer, &pointer_listener, &state);
        }
        state.keyboard = wl_seat_get_keyboard(state.seat);
        if (state.keyboard) {
            state.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
            wl_keyboard_add_listener(state.keyboard, &keyboard_listener, &state);
        }
    }

    // -------------------------------------------------------------------------
    // Create GPU device (Wayland surface)
    // -------------------------------------------------------------------------
    GPU::LinuxSurfaceInfo surfaceInfo{};
    surfaceInfo.display = display;
    surfaceInfo.window  = state.surface;
    surfaceInfo.api     = GPU::LinuxWindowApi::wayland;

    auto device = GPU::Device::createDefaultDevice(&surfaceInfo);
    if (!device) {
        std::cerr << "[Linux/Wayland] Failed to create campello_gpu device\n";
        wl_display_disconnect(display);
        return 1;
    }

    std::cerr << "[Linux/Wayland] Device: " << device->getName()
              << "  Engine: " << GPU::Device::getEngineVersion() << "\n";

    state.device = device;

    // -------------------------------------------------------------------------
    // Create dispatcher, focus manager, ticker, text input
    // -------------------------------------------------------------------------
    state.dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(state.dispatcher.get());

    state.focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(state.focus_manager.get());

    state.text_input_manager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(state.text_input_manager.get());

    state.ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(state.ticker_scheduler.get());

    // -------------------------------------------------------------------------
    // Set up IBus IME
    // -------------------------------------------------------------------------
    state.ibus_ime = std::make_unique<Widgets::IbusIme>();
    if (!state.ibus_ime->create()) {
        std::cerr << "[Linux/Wayland] IBus not available; IME composition disabled.\n";
    }

    state.text_input_manager->setOnInputTargetChanged(
        [&state](bool has_target) {
            if (!state.ibus_ime || !state.ibus_ime->isActive()) return;
            if (has_target) {
                state.ibus_ime->focusIn();
                updateImeCursorPosition(&state);
            } else {
                state.ibus_ime->focusOut();
            }
        });

    // -------------------------------------------------------------------------
    // FrameScheduler callback
    // -------------------------------------------------------------------------
    Widgets::FrameScheduler::setCallback([&state]() {
        state.needs_redraw = true;
    });

    // -------------------------------------------------------------------------
    // Wrap root widget with MediaQuery and mount
    // -------------------------------------------------------------------------
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = 1.0f;

    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(mediaData, root_widget);

    state.root_element = wrappedRoot->createElement();
    state.root_element->mount(nullptr);

    auto* roe = state.root_element->findDescendantRenderObjectElement();
    if (!roe) {
        std::cerr << "[Linux/Wayland] Widget tree produced no RenderObjectElement\n";
        return 1;
    }

    auto render_box = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!render_box) {
        std::cerr << "[Linux/Wayland] Root render object is not a RenderBox\n";
        return 1;
    }

    state.dispatcher->setRoot(render_box);

    // -------------------------------------------------------------------------
    // Create Vulkan draw backend and renderer
    // -------------------------------------------------------------------------
    const Widgets::Color bgColor = Widgets::Color::white();
    const GPU::PixelFormat pixelFmt = GPU::PixelFormat::bgra8unorm;

    auto backendOwned = std::make_unique<Widgets::VulkanDrawBackend>(
        device, bgColor, pixelFmt);

    state.renderer = std::make_shared<Widgets::Renderer>(
        device, render_box, bgColor);
    state.renderer->setDrawBackend(std::move(backendOwned));

    state.needs_redraw = true;

    // -------------------------------------------------------------------------
    // Main event loop
    // -------------------------------------------------------------------------
    while (state.running) {
        // Process all pending Wayland events
        wl_display_dispatch_pending(display);
        wl_display_flush(display);

        // Pump IBus D-Bus signals
        if (state.ibus_ime && state.ibus_ime->isActive()) {
            state.ibus_ime->dispatchEvents();
        }

        // Render if needed
        if (state.needs_redraw) {
            state.needs_redraw = false;
            renderFrame(&state);

            // Request next frame callback for vsync
            struct wl_callback* callback = wl_surface_frame(state.surface);
            wl_callback_add_listener(callback, &frame_listener, &state);
            wl_surface_damage(state.surface, 0, 0, INT32_MAX, INT32_MAX);
            wl_surface_commit(state.surface);
            wl_display_flush(display);
        }

        // Prepare to read/display events
        if (wl_display_prepare_read(display) == 0) {
            struct pollfd pfd = {};
            pfd.fd = wl_display_get_fd(display);
            pfd.events = POLLIN;

            int ret = poll(&pfd, 1, 16); // 16ms timeout (~60fps ticker)
            if (ret > 0) {
                wl_display_read_events(display);
            } else {
                wl_display_cancel_read(display);
                // Timeout: tick animations
                auto now = std::chrono::steady_clock::now();
                uint64_t now_ms = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count());
                if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
                if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);
            }
        } else {
            // Events already pending; next loop iteration will dispatch them
        }
    }

    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------
    state.ibus_ime.reset();

    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TextInputManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    state.renderer.reset();
    state.device.reset();

    if (state.xkb_state)  xkb_state_unref(state.xkb_state);
    if (state.xkb_keymap) xkb_keymap_unref(state.xkb_keymap);
    if (state.xkb_ctx)    xkb_context_unref(state.xkb_ctx);

    if (state.keyboard)   wl_keyboard_release(state.keyboard);
    if (state.pointer)    wl_pointer_release(state.pointer);
    if (state.seat)       wl_seat_destroy(state.seat);
    if (state.xdg_toplevel) xdg_toplevel_destroy(state.xdg_toplevel);
    if (state.xdg_surface)  xdg_surface_destroy(state.xdg_surface);
    if (state.surface)      wl_surface_destroy(state.surface);
    if (state.xdg_wm_base)  xdg_wm_base_destroy(state.xdg_wm_base);
    if (state.compositor)   wl_compositor_destroy(state.compositor);
    if (state.registry)     wl_registry_destroy(state.registry);

    wl_display_disconnect(display);
    return 0;
}

} // namespace systems::leal::campello_widgets

#endif // CAMPHELLO_WIDGETS_HAS_WAYLAND
