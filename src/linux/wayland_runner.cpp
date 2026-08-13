#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND

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
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/raster_thread.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/platform/linux_surface.hpp>

#include "ibus_ime.hpp"
#include "../gpu/vulkan/vulkan_draw_backend.hpp"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

// C interface installed by src/linux/platform_menu_delegate.cpp — must run
// before the widget tree mounts so PlatformMenuBar::build()'s setMenus()
// and PlatformMenuBarView's needsInWindowMenuBar() check reach the real
// LinuxPlatformMenuDelegate instead of the default no-op.
extern "C" void campello_widgets_initialize_linux_menu_delegate();

#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

#include <chrono>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <climits>
#include <cstdlib>
#include <utility>
#include <vector>
#include <libdecor.h>

// ---------------------------------------------------------------------------
// Crash handler: prints a backtrace to stderr on SIGSEGV/SIGABRT
// ---------------------------------------------------------------------------
static void crashHandler(int sig)
{
    void* frames[64];
    int   n = backtrace(frames, 64);
    const char* signame = (sig == SIGSEGV) ? "SIGSEGV" : "SIGABRT";
    // write() is async-signal-safe; fprintf is not, but backtrace_symbols_fd is.
    dprintf(STDERR_FILENO, "\n[CRASH] %s — backtrace (%d frames):\n", signame, n);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    dprintf(STDERR_FILENO, "[CRASH] Tip: addr2line -e ./build/linux-debug/campello_widgets_gallery <addr>\n");
    signal(sig, SIG_DFL);
    raise(sig);
}

// SIGINT / SIGTERM: request a clean shutdown via the event loop.
static volatile sig_atomic_t gInterruptRequested = 0;
static void interruptHandler(int) { gInterruptRequested = 1; }

namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------

// campello_gpu internal helper (not in device.hpp) — resizes the Wayland
// swapchain in-place without destroying the VkDevice.  Defined in
// campello_gpu/src/vulkan/device.cpp; resolved at link time.
extern "C" void campello_gpu_wayland_resize(uint32_t w, uint32_t h);

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

static bool isNavigationOrSpecialKeyWayland(Widgets::KeyCode key_code)
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
    struct wl_shm*        shm          = nullptr;
    struct libdecor*                        libdecor_ctx         = nullptr;
    struct libdecor_frame*                  decor_frame          = nullptr;

    // Cursor shape support — see the "Cursor theme + surface" block in
    // runAppWayland() for why this exists at all (short version: without
    // it, libdecor's own resize/drag cursor from the window border gets
    // stuck forever once the pointer moves into the content surface).
    struct wl_cursor_theme* cursor_theme      = nullptr;
    struct wl_surface*      cursor_surface    = nullptr;
    uint32_t                pointer_enter_serial = 0;

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
    std::unique_ptr<Widgets::RasterThread>      raster_thread;

    bool running             = true;
    bool needs_redraw        = true;
    bool needs_device_resize = false;
    bool configured          = false;
    bool closed              = false;
    int  width               = 800;
    int  height              = 600;
    int  device_width        = 0;    // size (physical px) at which the current GPU device was created
    int  device_height       = 0;
    bool mouse_pressed       = false;
    float last_pointer_x     = 0.0f;
    float last_pointer_y     = 0.0f;

    // HiDPI: outputs seen so far (wl_output*, scale factor from wl_output.scale),
    // the scale of whichever output wl_surface.enter last reported (0 = unknown
    // yet), and the scale actually applied via wl_surface_set_buffer_scale().
    std::vector<std::pair<struct wl_output*, int>> outputs;
    int  entered_scale       = 0;
    int  output_scale        = 1;
};

// ============================================================================
// Wayland listeners
// ============================================================================

// --- wl_output (HiDPI scale discovery) ---
static void output_geometry(void*, struct wl_output*, int32_t, int32_t, int32_t, int32_t,
                            int32_t, const char*, const char*, int32_t) {}
static void output_mode(void*, struct wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
static void output_done(void*, struct wl_output*) {}

static void output_scale_cb(void* data, struct wl_output* wl_output, int32_t factor)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    for (auto& entry : state->outputs) {
        if (entry.first == wl_output) { entry.second = factor; return; }
    }
    state->outputs.emplace_back(wl_output, factor);
}

// Trailing members (name/description, added in wl_output v4) are left
// zero-initialized — we bind at version <= 2, so the server never sends them.
static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode     = output_mode,
    .done     = output_done,
    .scale    = output_scale_cb,
};

// --- wl_surface (which output are we actually on right now?) ---
static void surface_enter(void* data, struct wl_surface*, struct wl_output* output)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    for (const auto& entry : state->outputs) {
        if (entry.first == output) { state->entered_scale = entry.second; return; }
    }
}
static void surface_leave(void*, struct wl_surface*, struct wl_output*) {}

static const struct wl_surface_listener surface_listener = {
    .enter = surface_enter,
    .leave = surface_leave,
};

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
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = static_cast<struct wl_shm*>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        // wl_output.scale was added in version 2; clamp to whatever the
        // compositor actually advertises so we don't request an unsupported
        // version on an old server.
        uint32_t bind_version = version < 2 ? version : 2;
        auto* output = static_cast<struct wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, bind_version));
        state->outputs.emplace_back(output, 1);
        wl_output_add_listener(output, &output_listener, state);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove
};

// --- libdecor context interface ---
static void libdecor_error_cb(struct libdecor*, enum libdecor_error, const char* msg)
{
    std::cerr << "[Linux/Wayland] libdecor error: " << msg << "\n";
}

static const struct libdecor_interface libdecor_iface = {
    libdecor_error_cb,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

// --- libdecor frame interface ---
static void frame_configure(struct libdecor_frame* frame,
    struct libdecor_configuration* config, void* user_data)
{
    auto* state = static_cast<WaylandWindowState*>(user_data);

    int w = state->width, h = state->height;
    libdecor_configuration_get_content_size(config, frame, &w, &h);
    if (w > 0) state->width  = w;
    if (h > 0) state->height = h;

    struct libdecor_state* ls = libdecor_state_new(state->width, state->height);
    libdecor_frame_commit(frame, ls, config);
    libdecor_state_free(ls);

    // If the device already exists and the window size changed, the Vulkan
    // swapchain must be recreated at the new dimensions. On Wayland,
    // vkGetPhysicalDeviceSurfaceCapabilitiesKHR always returns currentExtent
    // = UINT32_MAX, so the swapchain cannot self-detect the resize. We
    // recreate the device (analogous to Metal's automatic drawableSize update
    // on macOS) before the next frame.
    if (state->device &&
        (state->width  * state->output_scale != state->device_width ||
         state->height * state->output_scale != state->device_height)) {
        state->needs_device_resize = true;
    }

    state->configured   = true;
    state->needs_redraw = true;
}

static void frame_close(struct libdecor_frame*, void* user_data)
{
    auto* state = static_cast<WaylandWindowState*>(user_data);
    state->closed  = true;
    state->running = false;
}

static void frame_commit(struct libdecor_frame*, void* user_data)
{
    auto* state = static_cast<WaylandWindowState*>(user_data);
    // Commit the surface so libdecor can complete its configure sequence.
    // Also force a repaint so the raster thread submits a fresh buffer —
    // some compositors blank the surface when a commit arrives without one.
    if (state->renderer)
        state->renderer->notePaintRequested();
    wl_surface_commit(state->surface);
}

static void frame_dismiss_popup(struct libdecor_frame*, const char*, void*) {}

static const struct libdecor_frame_interface libdecor_frame_iface = {
    frame_configure,
    frame_close,
    frame_commit,
    frame_dismiss_popup,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

// --- Cursor shape helpers ---

// Tries the CSS cursor-shape name first (what modern cursor themes index
// by), falling back to the legacy Xcursor name older/minimal themes may
// only define.
static struct wl_cursor* loadThemeCursor(
    struct wl_cursor_theme* theme, const char* css_name, const char* legacy_name)
{
    struct wl_cursor* c = wl_cursor_theme_get_cursor(theme, css_name);
    if (!c && legacy_name) c = wl_cursor_theme_get_cursor(theme, legacy_name);
    return c;
}

static void waylandSetCursor(WaylandWindowState* state, Widgets::SystemMouseCursor cursor)
{
    if (!state->pointer || !state->cursor_theme || !state->cursor_surface) return;

    const char* css_name    = "default";
    const char* legacy_name = "left_ptr";
    switch (cursor) {
        case Widgets::SystemMouseCursor::pointer:
            css_name = "pointer"; legacy_name = "hand2"; break;
        case Widgets::SystemMouseCursor::text:
            css_name = "text"; legacy_name = "xterm"; break;
        case Widgets::SystemMouseCursor::forbidden:
            css_name = "not-allowed"; legacy_name = "crossed_circle"; break;
        case Widgets::SystemMouseCursor::resize_ns:
            css_name = "ns-resize"; legacy_name = "sb_v_double_arrow"; break;
        case Widgets::SystemMouseCursor::resize_ew:
            css_name = "ew-resize"; legacy_name = "sb_h_double_arrow"; break;
        default:
            break;
    }

    struct wl_cursor* wl_cur = loadThemeCursor(state->cursor_theme, css_name, legacy_name);
    if (!wl_cur || wl_cur->image_count == 0) return;

    struct wl_cursor_image* image  = wl_cur->images[0];
    struct wl_buffer*       buffer = wl_cursor_image_get_buffer(image);
    if (!buffer) return;

    wl_surface_attach(state->cursor_surface, buffer, 0, 0);
    // Damaging the whole surface (rather than computing the exact
    // scaled/logical rect) is the standard simplification for cursor
    // surfaces — they're tiny, so there's no real cost to it.
    wl_surface_damage(state->cursor_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(state->cursor_surface);

    // Hotspot must be in surface-local (logical) coordinates; the theme
    // was loaded at a physical-pixel size (see runAppWayland()), so divide
    // back out by the same scale set via wl_surface_set_buffer_scale().
    const int32_t hotspot_x = static_cast<int32_t>(image->hotspot_x) / state->output_scale;
    const int32_t hotspot_y = static_cast<int32_t>(image->hotspot_y) / state->output_scale;
    wl_pointer_set_cursor(state->pointer, state->pointer_enter_serial,
        state->cursor_surface, hotspot_x, hotspot_y);
}

// --- wl_pointer ---
static void pointer_enter(void* data, struct wl_pointer* pointer,
                          uint32_t serial, struct wl_surface* surface,
                          wl_fixed_t sx, wl_fixed_t sy)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    state->pointer_enter_serial = serial;
    state->last_pointer_x = static_cast<float>(wl_fixed_to_double(sx));
    state->last_pointer_y = static_cast<float>(wl_fixed_to_double(sy));

    // Reclaim the cursor immediately rather than waiting for the pointer
    // to move — otherwise whatever libdecor left set on its own resize/
    // drag border stays visible until the pointer first crosses a
    // MouseRegion boundary inside the content (see waylandSetCursor()'s
    // registration in runAppWayland() for the full story). Also drives
    // hit-testing right away so hover-dependent state is correct on entry.
    waylandSetCursor(state, Widgets::SystemMouseCursor::arrow);

    if (state->dispatcher) {
        Widgets::PointerEvent e;
        e.kind       = Widgets::PointerEventKind::move;
        e.pointer_id = 0;
        e.position   = { state->last_pointer_x, state->last_pointer_y };
        e.pressure   = state->mouse_pressed ? 1.0f : 0.0f;
        state->dispatcher->handlePointerEvent(e);
    }
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

static void pointer_axis(void* data, struct wl_pointer* /*pointer*/,
                         uint32_t /*time*/, uint32_t axis, wl_fixed_t value)
{
    auto* state = static_cast<WaylandWindowState*>(data);
    if (!state->dispatcher) return;

    const float delta = static_cast<float>(wl_fixed_to_double(value));

    Widgets::PointerEvent e;
    e.kind       = Widgets::PointerEventKind::scroll;
    e.pointer_id = 0;
    e.position   = { state->last_pointer_x, state->last_pointer_y };
    e.pressure   = 0.0f;

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        e.scroll_delta_y = delta;
    else
        e.scroll_delta_x = delta;

    state->dispatcher->handlePointerEvent(e);
}

// wl_pointer version 5+ events — not used, required for listener completeness.
static void pointer_frame(void*, struct wl_pointer*) {}
static void pointer_axis_source(void*, struct wl_pointer*, uint32_t) {}
static void pointer_axis_stop(void*, struct wl_pointer*, uint32_t, uint32_t) {}
static void pointer_axis_discrete(void*, struct wl_pointer*, uint32_t, int32_t) {}
static void pointer_axis_value120(void*, struct wl_pointer*, uint32_t, int32_t) {}
static void pointer_axis_relative_direction(void*, struct wl_pointer*, uint32_t, uint32_t) {}

static const struct wl_pointer_listener pointer_listener = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
    pointer_frame,
    pointer_axis_source,
    pointer_axis_stop,
    pointer_axis_discrete,
    pointer_axis_value120,
    pointer_axis_relative_direction,
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
        if (isNavigationOrSpecialKeyWayland(key_code)) {
            Widgets::KeyEvent ke;
            ke.kind      = Widgets::KeyEventKind::down;
            ke.key_code  = key_code;
            ke.modifiers = mods;
            ke.character = 0;
            ws->focus_manager->handleKeyEvent(ke);
            return;
        }

        // A Ctrl-held keystroke is never text composition input — it's
        // always an app/system command by convention — so it must still
        // reach FocusManager::handleKeyEvent() below even while a text
        // field has IBus's input target, or app-wide Ctrl+<key> shortcuts
        // (see FocusManager::setGlobalKeyHandler()) would silently stop
        // firing the moment any TextField gains focus.
        bool consumed_by_ime = false;
        if (ws->ibus_ime && ws->ibus_ime->isActive() &&
            ws->text_input_manager && ws->text_input_manager->hasInputTarget() &&
            !(mods & Widgets::KeyModifiers::ctrl))
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

// wl_keyboard version 3+: key repeat rate/delay; we don't implement key repeat.
static void keyboard_repeat_info(void* data, struct wl_keyboard* keyboard,
                                 int32_t rate, int32_t delay) {}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info
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

static bool renderFrame(WaylandWindowState* state)
{
    if (!state || !state->renderer || !state->device) return false;

    auto package = state->renderer->buildFrame(
        static_cast<float>(state->width  * state->output_scale),
        static_cast<float>(state->height * state->output_scale));

    if (!package) return false;

    // On Vulkan, getSwapchainTextureView() returns nullptr — campello_gpu's
    // beginRenderPass acquires the swapchain image automatically when target is null.
    package->target = state->device->getSwapchainTextureView();

    state->raster_thread->submit(std::move(*package));
    return true;
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

    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGINT,  interruptHandler);
    signal(SIGTERM, interruptHandler);

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

    // -------------------------------------------------------------------------
    // Create wl_surface and decorate it with libdecor
    // libdecor creates the xdg_surface + xdg_toplevel internally, negotiates
    // server-side decorations when available, and draws GNOME-style client-side
    // decorations as a fallback.
    // -------------------------------------------------------------------------
    state.surface = wl_compositor_create_surface(state.compositor);
    if (!state.surface) {
        std::cerr << "[Linux/Wayland] Failed to create wl_surface\n";
        wl_display_disconnect(display);
        return 1;
    }
    wl_surface_add_listener(state.surface, &surface_listener, &state);

    state.libdecor_ctx = libdecor_new(display,
        const_cast<struct libdecor_interface*>(&libdecor_iface));
    if (!state.libdecor_ctx) {
        std::cerr << "[Linux/Wayland] Failed to create libdecor context\n";
        wl_surface_destroy(state.surface);
        wl_display_disconnect(display);
        return 1;
    }

    state.decor_frame = libdecor_decorate(state.libdecor_ctx, state.surface,
        const_cast<struct libdecor_frame_interface*>(&libdecor_frame_iface), &state);
    if (!state.decor_frame) {
        std::cerr << "[Linux/Wayland] Failed to decorate surface\n";
        libdecor_unref(state.libdecor_ctx);
        wl_surface_destroy(state.surface);
        wl_display_disconnect(display);
        return 1;
    }

    libdecor_frame_set_title(state.decor_frame, title.c_str());
    libdecor_frame_set_capabilities(state.decor_frame,
        static_cast<enum libdecor_capabilities>(
            LIBDECOR_ACTION_MOVE     |
            LIBDECOR_ACTION_RESIZE   |
            LIBDECOR_ACTION_MINIMIZE |
            LIBDECOR_ACTION_CLOSE));
    libdecor_frame_map(state.decor_frame);

    // Wait for the first configure event (libdecor dispatches from its own
    // internal queue — this replaces the raw wl_display_dispatch wait).
    while (!state.configured && state.running) {
        if (libdecor_dispatch(state.libdecor_ctx, 16) < 0) break;
    }
    if (!state.running) {
        libdecor_frame_unref(state.decor_frame);
        libdecor_unref(state.libdecor_ctx);
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
    // Determine the compositor output scale (HiDPI support)
    // -------------------------------------------------------------------------
    // wl_surface.enter (which output we actually ended up on) typically
    // arrives shortly after the first commit that the configure-wait loop
    // above already performed — one more roundtrip flushes it, along with
    // any wl_output.scale events still in flight, before we size the GPU
    // surface. Falls back to the first known output, then to 1x, if the
    // compositor never sends an enter event (e.g. single-output setups on
    // some compositors).
    wl_display_roundtrip(display);

    state.output_scale = state.entered_scale > 0 ? state.entered_scale
                        : (!state.outputs.empty() ? state.outputs.front().second : 1);
    if (state.output_scale < 1) state.output_scale = 1;
    wl_surface_set_buffer_scale(state.surface, state.output_scale);

    if (state.output_scale != 1) {
        std::cerr << "[Linux/Wayland] Output scale: " << state.output_scale << "x\n";
    }

    // -------------------------------------------------------------------------
    // Cursor theme + surface, and register the cursor-change handler
    // -------------------------------------------------------------------------
    // Without this, MouseRegion/TextField/etc. cursor changes are silent
    // no-ops on Wayland (setSystemCursor() just calls into a null handler —
    // see system_mouse_cursor.cpp), and — because libdecor manages its own
    // cursor for the window's title bar/resize border, and the Wayland
    // protocol has no automatic reset when the pointer crosses from a
    // decoration into the content surface — whatever resize/drag cursor
    // libdecor last set while the pointer crossed the border simply
    // persists forever once inside, with nothing to ever change it.
    if (state.shm && state.pointer) {
        state.cursor_theme = wl_cursor_theme_load(nullptr, 24 * state.output_scale, state.shm);
        if (state.cursor_theme) {
            state.cursor_surface = wl_compositor_create_surface(state.compositor);
            if (state.cursor_surface) {
                wl_surface_set_buffer_scale(state.cursor_surface, state.output_scale);
                Widgets::registerCursorHandler([&state](Widgets::SystemMouseCursor c) {
                    waylandSetCursor(&state, c);
                });
            } else {
                std::cerr << "[Linux/Wayland] Failed to create cursor surface; cursor shape changes disabled.\n";
            }
        } else {
            std::cerr << "[Linux/Wayland] Failed to load cursor theme; cursor shape changes disabled.\n";
        }
    } else {
        std::cerr << "[Linux/Wayland] wl_shm or wl_pointer unavailable; cursor shape changes disabled.\n";
    }

    // -------------------------------------------------------------------------
    // Create GPU device (Wayland surface)
    // -------------------------------------------------------------------------
    GPU::LinuxSurfaceInfo surfaceInfo{};
    surfaceInfo.display = display;
    surfaceInfo.window  = state.surface;
    surfaceInfo.api     = GPU::LinuxWindowApi::wayland;
    surfaceInfo.width   = static_cast<uint32_t>(state.width  * state.output_scale);
    surfaceInfo.height  = static_cast<uint32_t>(state.height * state.output_scale);

    auto device = GPU::Device::createDefaultDevice(&surfaceInfo);
    if (!device) {
        std::cerr << "[Linux/Wayland] Failed to create campello_gpu device; will fall back to X11.\n";
        // The registered handler (if any) captured &state by reference —
        // clear it before state goes out of scope, or X11's fallback path
        // could observe a dangling handler for the brief window before it
        // installs its own.
        Widgets::registerCursorHandler(nullptr);
        wl_display_disconnect(display);
        return 2; // sentinel: GPU init failed, caller should retry with X11
    }

    std::cerr << "[Linux/Wayland] Device: " << device->getName()
              << "  Engine: " << GPU::Device::getEngineVersion() << "\n";

    state.device        = device;
    state.device_width  = state.width  * state.output_scale;
    state.device_height = state.height * state.output_scale;

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
    mediaData.device_pixel_ratio = static_cast<float>(state.output_scale);
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(state.width),
        static_cast<float>(state.height) };

    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(mediaData, root_widget);

    // Bind the UI thread before any widget tree mutation.
    Widgets::ThreadChecker::instance().bindToCurrentThread();

    campello_widgets_initialize_linux_menu_delegate();

    std::cerr << "[Linux/Wayland] Mounting widget tree\n";
    state.root_element = wrappedRoot->createElement();
    state.root_element->mount(nullptr);
    std::cerr << "[Linux/Wayland] Widget tree mounted\n";

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
    std::cerr << "[Linux/Wayland] RenderBox found\n";

    state.dispatcher->setRoot(render_box);

    // -------------------------------------------------------------------------
    // Create Vulkan draw backend and renderer
    // -------------------------------------------------------------------------
    const Widgets::Color bgColor = Widgets::Color::white();
    const GPU::PixelFormat pixelFmt = GPU::PixelFormat::bgra8unorm;

    std::cerr << "[Linux/Wayland] Creating VulkanDrawBackend\n";
    auto backendOwned = std::make_unique<Widgets::VulkanDrawBackend>(
        device, bgColor, pixelFmt);
    std::cerr << "[Linux/Wayland] VulkanDrawBackend created\n";

    state.renderer = std::make_shared<Widgets::Renderer>(
        device, render_box, bgColor);
    state.renderer->setDrawBackend(std::move(backendOwned));
    // Renderer::device_pixel_ratio_ is distinct from MediaQueryData's copy —
    // it's what buildFrame() actually divides the physical viewport by to
    // get logical layout constraints. Without this, layout sizes everything
    // against the full physical viewport as if DPR were 1, shrinking every
    // fixed-size widget to roughly 1/scale of its intended on-screen size.
    state.renderer->setDevicePixelRatio(static_cast<float>(state.output_scale));
    std::cerr << "[Linux/Wayland] Renderer ready\n";

    state.raster_thread = std::make_unique<Widgets::RasterThread>(
        [renderer = state.renderer](const Widgets::FramePackage& pkg) {
            renderer->rasterFrame(pkg);
        });
    state.raster_thread->start();
    std::cerr << "[Linux/Wayland] Raster thread started, entering event loop\n";

    state.needs_redraw = true;

    // -------------------------------------------------------------------------
    // Main event loop
    // -------------------------------------------------------------------------
    while (state.running) {
        // Process all pending Wayland events
        if (gInterruptRequested) { state.running = false; continue; }

        wl_display_dispatch_pending(display);
        // Dispatch libdecor events (decoration redraws, resize negotiations, etc.)
        libdecor_dispatch(state.libdecor_ctx, 0);
        wl_display_flush(display);

        // Pump IBus D-Bus signals
        if (state.ibus_ime && state.ibus_ime->isActive()) {
            state.ibus_ime->dispatchEvents();
        }

        // Resize the Vulkan swapchain when the window size changed. On Wayland,
        // vkGetPhysicalDeviceSurfaceCapabilitiesKHR always returns currentExtent
        // = UINT32_MAX, so the swapchain cannot self-detect the new dimensions.
        // campello_gpu_wayland_resize() patches imageExtent and calls
        // recreateSwapchain() internally, keeping the VkDevice alive —
        // analogous to Metal's transparent drawableSize update on macOS.
        if (state.needs_device_resize && state.device) {
            state.needs_device_resize = false;
            // Wait for the raster thread to finish its current frame before
            // recreating the swapchain — destroying swapchain images while
            // the raster thread still holds a command buffer recording against
            // them causes a Vulkan validation error and crash.
            if (state.raster_thread)
                state.raster_thread->drain();
            campello_gpu_wayland_resize(
                static_cast<uint32_t>(state.width  * state.output_scale),
                static_cast<uint32_t>(state.height * state.output_scale));
            state.device_width  = state.width  * state.output_scale;
            state.device_height = state.height * state.output_scale;
        }

        // Render if needed
        if (state.needs_redraw && state.renderer) {
            state.needs_redraw = false;
            bool submitted = renderFrame(&state);

            // Only register the vsync frame callback and commit the surface when
            // we actually submitted a frame to the GPU. On Vulkan, vkQueuePresentKHR
            // (called from the raster thread) manages wl_surface_attach + commit
            // internally. Committing the surface here without a new buffer when no
            // frame was submitted just confuses the compositor.
            if (submitted) {
                struct wl_callback* callback = wl_surface_frame(state.surface);
                wl_callback_add_listener(callback, &frame_listener, &state);
                wl_display_flush(display);
            }
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
            }
        } else {
            // Events already pending; next loop iteration will dispatch them
        }

        // Tick animations/physics every loop iteration, not only when
        // poll() times out with no Wayland events pending. frame_done()
        // (this window's vsync-pacing callback, above) unconditionally
        // requests another redraw on every compositor frame callback once
        // any animation has rendered even one frame — and that callback
        // arrives as a Wayland event, which almost always wins the race
        // against poll()'s 16ms timeout. Ticking only in the timeout
        // branch meant a fling or spring-back animation got exactly one
        // tick (whichever one happened to slip through when timing lined
        // up), then never ticked again even though frames kept being
        // redrawn — the animation looked like it froze instead of
        // continuing, because nothing was left to advance its state.
        auto now = std::chrono::steady_clock::now();
        uint64_t now_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
        if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
        if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);
    }

    // -------------------------------------------------------------------------
    // Cleanup — order matters:
    //   1. Stop scheduling / dispatch first to prevent stale callbacks.
    //   2. Tear down the widget / element tree while managers are still live
    //      (elements may call activeDispatcher() or activeManager() on detach).
    //   3. Release GPU objects before the Wayland surface is destroyed, so
    //      the Vulkan WSI can clean up its Wayland resources while the surface
    //      is still valid.
    //   4. Wayland object destruction: children before parents, libdecor before
    //      wl_surface (libdecor owns xdg_surface / xdg_toplevel on top of it).
    // -------------------------------------------------------------------------

    // Stop frame scheduling immediately — the lambda captures &state and must
    // not fire after runAppWayland returns.
    Widgets::FrameScheduler::setCallback({});

    state.ibus_ime.reset();

    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TextInputManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    // Captured &state by reference — must be cleared before state's
    // Wayland objects below start getting torn down.
    Widgets::registerCursorHandler(nullptr);

    // Unmount the element / render-object tree while managers are still reachable
    // (RenderTextField::detach() checks activeDispatcher(), etc.).
    state.root_element.reset();

    // Stop the raster thread before releasing the renderer — stop() blocks
    // until any in-flight rasterFrame() call completes and joins the thread,
    // so no raster work can ever observe a half-destroyed Renderer/Device.
    state.raster_thread.reset();

    state.renderer.reset();

    // Release everything else that can hold GPU-backed textures before the
    // device is destroyed. render_box is a local variable that would otherwise
    // outlive state.device; state.dispatcher holds a ref to it via setRoot().
    // OffsetLayer caches DrawLists that contain DrawImageCmd/DrawTextCmd with
    // shared_ptr<Texture> — those textures call vkFreeMemory in their dtor.
    render_box.reset();
    state.dispatcher.reset();
    state.focus_manager.reset();
    Widgets::ImageCache::instance().clear();

    state.device.reset();
    // The local `device` variable also holds a ref to the GPU::Device; reset it
    // here so vkDestroySurfaceKHR fires while the Wayland display is still open.
    // On hasvk/Wayland, vkDestroySurfaceKHR sends a null-commit to the compositor
    // — if the display is already disconnected that commit crashes the WSI.
    device.reset();

    if (state.xkb_state)  xkb_state_unref(state.xkb_state);
    if (state.xkb_keymap) xkb_keymap_unref(state.xkb_keymap);
    if (state.xkb_ctx)    xkb_context_unref(state.xkb_ctx);

    if (state.cursor_surface) wl_surface_destroy(state.cursor_surface);
    if (state.cursor_theme)   wl_cursor_theme_destroy(state.cursor_theme);
    if (state.keyboard)     wl_keyboard_release(state.keyboard);
    if (state.pointer)      wl_pointer_release(state.pointer);
    // wl_seat version ≥5 requires the release request (not just proxy destroy).
    // We bind version 7, so wl_seat_release is mandatory.
    if (state.seat)         wl_seat_release(state.seat);
    if (state.decor_frame)  libdecor_frame_unref(state.decor_frame);
    if (state.libdecor_ctx) libdecor_unref(state.libdecor_ctx);
    if (state.surface)      wl_surface_destroy(state.surface);
    for (auto& entry : state.outputs) wl_output_destroy(entry.first);
    if (state.compositor)   wl_compositor_destroy(state.compositor);
    if (state.registry)     wl_registry_destroy(state.registry);

    wl_display_disconnect(display);
    return 0;
}

} // namespace systems::leal::campello_widgets

#endif // CAMPELLO_WIDGETS_HAS_WAYLAND
