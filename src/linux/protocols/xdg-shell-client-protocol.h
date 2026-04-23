/* Auto-generated from xdg-shell.xml -- do not edit. */
#pragma once

#include <wayland-client.h>

extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_popup_interface;

enum xdg_wm_base_request {
    XDG_WM_BASE_DESTROY = 0,
    XDG_WM_BASE_CREATE_POSITIONER = 1,
    XDG_WM_BASE_GET_XDG_SURFACE = 2,
    XDG_WM_BASE_PONG = 3,
    XDG_WM_BASE_REQUEST_COUNT = 4
};

enum xdg_wm_base_event {
    XDG_WM_BASE_PING = 0,
    XDG_WM_BASE_EVENT_COUNT = 1
};

enum xdg_wm_base_ERROR {
    XDG_WM_BASE_ERROR_ROLE = 0,
    XDG_WM_BASE_ERROR_DEFUNCT_SURFACES = 1,
    XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP = 2,
    XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT = 3,
    XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE = 4,
    XDG_WM_BASE_ERROR_INVALID_POSITIONER = 5,
    XDG_WM_BASE_ERROR_UNRESPONSIVE = 6,
};

static const struct wl_message xdg_wm_base_requests[] = {
    { "destroy", "", NULL },
    { "create_positioner", "n", (const struct wl_interface *[]){ &xdg_positioner_interface } },
    { "get_xdg_surface", "no", (const struct wl_interface *[]){ &xdg_surface_interface, &wl_surface_interface } },
    { "pong", "u", (const struct wl_interface *[]){ NULL } },
};

static const struct wl_message xdg_wm_base_events[] = {
    { "ping", "u", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface xdg_wm_base_interface = {
    "xdg_wm_base", 7,
    4, xdg_wm_base_requests, 1, xdg_wm_base_events
};

static inline void xdg_wm_base_destroy(struct xdg_wm_base *xdg_wm_base) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_wm_base, XDG_WM_BASE_DESTROY, NULL, 0, 0);
}

static inline struct xdg_positioner *xdg_wm_base_create_positioner(struct xdg_wm_base *xdg_wm_base) {
    return (struct xdg_positioner *)wl_proxy_marshal_flags(
        (struct wl_proxy*)xdg_wm_base, XDG_WM_BASE_CREATE_POSITIONER, &xdg_positioner_interface, 1, 0);
}

static inline struct xdg_surface *xdg_wm_base_get_xdg_surface(struct xdg_wm_base *xdg_wm_base, struct wl_surface *surface) {
    return (struct xdg_surface *)wl_proxy_marshal_flags(
        (struct wl_proxy*)xdg_wm_base, XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface, 1, 0, (struct wl_proxy*)surface);
}

static inline void xdg_wm_base_pong(struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_wm_base, XDG_WM_BASE_PONG, NULL, 0, 0, serial);
}

struct xdg_wm_base_listener {
    void (*ping)(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
};

static inline int xdg_wm_base_add_listener(struct xdg_wm_base *xdg_wm_base,
    const struct xdg_wm_base_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy*)xdg_wm_base,
        (void (**)(void))listener, data);
}

enum xdg_positioner_request {
    XDG_POSITIONER_DESTROY = 0,
    XDG_POSITIONER_SET_SIZE = 1,
    XDG_POSITIONER_SET_ANCHOR_RECT = 2,
    XDG_POSITIONER_SET_ANCHOR = 3,
    XDG_POSITIONER_SET_GRAVITY = 4,
    XDG_POSITIONER_SET_CONSTRAINT_ADJUSTMENT = 5,
    XDG_POSITIONER_SET_OFFSET = 6,
    XDG_POSITIONER_SET_REACTIVE = 7,
    XDG_POSITIONER_SET_PARENT_SIZE = 8,
    XDG_POSITIONER_SET_PARENT_CONFIGURE = 9,
    XDG_POSITIONER_REQUEST_COUNT = 10
};

enum xdg_positioner_ERROR {
    XDG_POSITIONER_ERROR_INVALID_INPUT = 0,
};

enum xdg_positioner_ANCHOR {
    XDG_POSITIONER_ANCHOR_NONE = 0,
    XDG_POSITIONER_ANCHOR_TOP = 1,
    XDG_POSITIONER_ANCHOR_BOTTOM = 2,
    XDG_POSITIONER_ANCHOR_LEFT = 3,
    XDG_POSITIONER_ANCHOR_RIGHT = 4,
    XDG_POSITIONER_ANCHOR_TOP_LEFT = 5,
    XDG_POSITIONER_ANCHOR_BOTTOM_LEFT = 6,
    XDG_POSITIONER_ANCHOR_TOP_RIGHT = 7,
    XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT = 8,
};

enum xdg_positioner_GRAVITY {
    XDG_POSITIONER_GRAVITY_NONE = 0,
    XDG_POSITIONER_GRAVITY_TOP = 1,
    XDG_POSITIONER_GRAVITY_BOTTOM = 2,
    XDG_POSITIONER_GRAVITY_LEFT = 3,
    XDG_POSITIONER_GRAVITY_RIGHT = 4,
    XDG_POSITIONER_GRAVITY_TOP_LEFT = 5,
    XDG_POSITIONER_GRAVITY_BOTTOM_LEFT = 6,
    XDG_POSITIONER_GRAVITY_TOP_RIGHT = 7,
    XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT = 8,
};

enum xdg_positioner_CONSTRAINT_ADJUSTMENT {
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE = 0,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X = 1,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y = 2,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X = 4,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y = 8,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X = 16,
    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y = 32,
};

static const struct wl_message xdg_positioner_requests[] = {
    { "destroy", "", NULL },
    { "set_size", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_anchor_rect", "iiii", (const struct wl_interface *[]){ NULL, NULL, NULL, NULL } },
    { "set_anchor", "u", (const struct wl_interface *[]){ NULL } },
    { "set_gravity", "u", (const struct wl_interface *[]){ NULL } },
    { "set_constraint_adjustment", "u", (const struct wl_interface *[]){ NULL } },
    { "set_offset", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_reactive", "", NULL },
    { "set_parent_size", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_parent_configure", "u", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface xdg_positioner_interface = {
    "xdg_positioner", 7,
    10, xdg_positioner_requests, 0, NULL
};

static inline void xdg_positioner_destroy(struct xdg_positioner *xdg_positioner) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_DESTROY, NULL, 0, 0);
}

static inline void xdg_positioner_set_size(struct xdg_positioner *xdg_positioner, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_SIZE, NULL, 0, 0, width, height);
}

static inline void xdg_positioner_set_anchor_rect(struct xdg_positioner *xdg_positioner, int32_t x, int32_t y, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_ANCHOR_RECT, NULL, 0, 0, x, y, width, height);
}

static inline void xdg_positioner_set_anchor(struct xdg_positioner *xdg_positioner, uint32_t anchor) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_ANCHOR, NULL, 0, 0, anchor);
}

static inline void xdg_positioner_set_gravity(struct xdg_positioner *xdg_positioner, uint32_t gravity) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_GRAVITY, NULL, 0, 0, gravity);
}

static inline void xdg_positioner_set_constraint_adjustment(struct xdg_positioner *xdg_positioner, uint32_t constraint_adjustment) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_CONSTRAINT_ADJUSTMENT, NULL, 0, 0, constraint_adjustment);
}

static inline void xdg_positioner_set_offset(struct xdg_positioner *xdg_positioner, int32_t x, int32_t y) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_OFFSET, NULL, 0, 0, x, y);
}

static inline void xdg_positioner_set_reactive(struct xdg_positioner *xdg_positioner) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_REACTIVE, NULL, 0, 0);
}

static inline void xdg_positioner_set_parent_size(struct xdg_positioner *xdg_positioner, int32_t parent_width, int32_t parent_height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_PARENT_SIZE, NULL, 0, 0, parent_width, parent_height);
}

static inline void xdg_positioner_set_parent_configure(struct xdg_positioner *xdg_positioner, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_positioner, XDG_POSITIONER_SET_PARENT_CONFIGURE, NULL, 0, 0, serial);
}

enum xdg_surface_request {
    XDG_SURFACE_DESTROY = 0,
    XDG_SURFACE_GET_TOPLEVEL = 1,
    XDG_SURFACE_GET_POPUP = 2,
    XDG_SURFACE_SET_WINDOW_GEOMETRY = 3,
    XDG_SURFACE_ACK_CONFIGURE = 4,
    XDG_SURFACE_REQUEST_COUNT = 5
};

enum xdg_surface_event {
    XDG_SURFACE_CONFIGURE = 0,
    XDG_SURFACE_EVENT_COUNT = 1
};

enum xdg_surface_ERROR {
    XDG_SURFACE_ERROR_NOT_CONSTRUCTED = 1,
    XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED = 2,
    XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER = 3,
    XDG_SURFACE_ERROR_INVALID_SERIAL = 4,
    XDG_SURFACE_ERROR_INVALID_SIZE = 5,
    XDG_SURFACE_ERROR_DEFUNCT_ROLE_OBJECT = 6,
};

static const struct wl_message xdg_surface_requests[] = {
    { "destroy", "", NULL },
    { "get_toplevel", "n", (const struct wl_interface *[]){ &xdg_toplevel_interface } },
    { "get_popup", "noo", (const struct wl_interface *[]){ &xdg_popup_interface, &xdg_surface_interface, &xdg_positioner_interface } },
    { "set_window_geometry", "iiii", (const struct wl_interface *[]){ NULL, NULL, NULL, NULL } },
    { "ack_configure", "u", (const struct wl_interface *[]){ NULL } },
};

static const struct wl_message xdg_surface_events[] = {
    { "configure", "u", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface xdg_surface_interface = {
    "xdg_surface", 7,
    5, xdg_surface_requests, 1, xdg_surface_events
};

static inline void xdg_surface_destroy(struct xdg_surface *xdg_surface) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_surface, XDG_SURFACE_DESTROY, NULL, 0, 0);
}

static inline struct xdg_toplevel *xdg_surface_get_toplevel(struct xdg_surface *xdg_surface) {
    return (struct xdg_toplevel *)wl_proxy_marshal_flags(
        (struct wl_proxy*)xdg_surface, XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, 1, 0);
}

static inline struct xdg_popup *xdg_surface_get_popup(struct xdg_surface *xdg_surface, struct xdg_surface *parent, struct xdg_positioner *positioner) {
    return (struct xdg_popup *)wl_proxy_marshal_flags(
        (struct wl_proxy*)xdg_surface, XDG_SURFACE_GET_POPUP, &xdg_popup_interface, 1, 0, (struct wl_proxy*)parent, (struct wl_proxy*)positioner);
}

static inline void xdg_surface_set_window_geometry(struct xdg_surface *xdg_surface, int32_t x, int32_t y, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_surface, XDG_SURFACE_SET_WINDOW_GEOMETRY, NULL, 0, 0, x, y, width, height);
}

static inline void xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_surface, XDG_SURFACE_ACK_CONFIGURE, NULL, 0, 0, serial);
}

struct xdg_surface_listener {
    void (*configure)(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
};

static inline int xdg_surface_add_listener(struct xdg_surface *xdg_surface,
    const struct xdg_surface_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy*)xdg_surface,
        (void (**)(void))listener, data);
}

enum xdg_toplevel_request {
    XDG_TOPLEVEL_DESTROY = 0,
    XDG_TOPLEVEL_SET_PARENT = 1,
    XDG_TOPLEVEL_SET_TITLE = 2,
    XDG_TOPLEVEL_SET_APP_ID = 3,
    XDG_TOPLEVEL_SHOW_WINDOW_MENU = 4,
    XDG_TOPLEVEL_MOVE = 5,
    XDG_TOPLEVEL_RESIZE = 6,
    XDG_TOPLEVEL_SET_MAX_SIZE = 7,
    XDG_TOPLEVEL_SET_MIN_SIZE = 8,
    XDG_TOPLEVEL_SET_MAXIMIZED = 9,
    XDG_TOPLEVEL_UNSET_MAXIMIZED = 10,
    XDG_TOPLEVEL_SET_FULLSCREEN = 11,
    XDG_TOPLEVEL_UNSET_FULLSCREEN = 12,
    XDG_TOPLEVEL_SET_MINIMIZED = 13,
    XDG_TOPLEVEL_REQUEST_COUNT = 14
};

enum xdg_toplevel_event {
    XDG_TOPLEVEL_CONFIGURE = 0,
    XDG_TOPLEVEL_CLOSE = 1,
    XDG_TOPLEVEL_CONFIGURE_BOUNDS = 2,
    XDG_TOPLEVEL_WM_CAPABILITIES = 3,
    XDG_TOPLEVEL_EVENT_COUNT = 4
};

enum xdg_toplevel_ERROR {
    XDG_TOPLEVEL_ERROR_INVALID_RESIZE_EDGE = 0,
    XDG_TOPLEVEL_ERROR_INVALID_PARENT = 1,
    XDG_TOPLEVEL_ERROR_INVALID_SIZE = 2,
};

enum xdg_toplevel_RESIZE_EDGE {
    XDG_TOPLEVEL_RESIZE_EDGE_NONE = 0,
    XDG_TOPLEVEL_RESIZE_EDGE_TOP = 1,
    XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM = 2,
    XDG_TOPLEVEL_RESIZE_EDGE_LEFT = 4,
    XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT = 5,
    XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT = 6,
    XDG_TOPLEVEL_RESIZE_EDGE_RIGHT = 8,
    XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT = 9,
    XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT = 10,
};

enum xdg_toplevel_STATE {
    XDG_TOPLEVEL_STATE_MAXIMIZED = 1,
    XDG_TOPLEVEL_STATE_FULLSCREEN = 2,
    XDG_TOPLEVEL_STATE_RESIZING = 3,
    XDG_TOPLEVEL_STATE_ACTIVATED = 4,
    XDG_TOPLEVEL_STATE_TILED_LEFT = 5,
    XDG_TOPLEVEL_STATE_TILED_RIGHT = 6,
    XDG_TOPLEVEL_STATE_TILED_TOP = 7,
    XDG_TOPLEVEL_STATE_TILED_BOTTOM = 8,
    XDG_TOPLEVEL_STATE_SUSPENDED = 9,
    XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT = 10,
    XDG_TOPLEVEL_STATE_CONSTRAINED_RIGHT = 11,
    XDG_TOPLEVEL_STATE_CONSTRAINED_TOP = 12,
    XDG_TOPLEVEL_STATE_CONSTRAINED_BOTTOM = 13,
};

enum xdg_toplevel_WM_CAPABILITIES {
    XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU = 1,
    XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE = 2,
    XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN = 3,
    XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE = 4,
};

static const struct wl_message xdg_toplevel_requests[] = {
    { "destroy", "", NULL },
    { "set_parent", "o", (const struct wl_interface *[]){ &xdg_toplevel_interface } },
    { "set_title", "s", (const struct wl_interface *[]){ NULL } },
    { "set_app_id", "s", (const struct wl_interface *[]){ NULL } },
    { "show_window_menu", "ouii", (const struct wl_interface *[]){ &wl_seat_interface, NULL, NULL, NULL } },
    { "move", "ou", (const struct wl_interface *[]){ &wl_seat_interface, NULL } },
    { "resize", "ouu", (const struct wl_interface *[]){ &wl_seat_interface, NULL, NULL } },
    { "set_max_size", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_min_size", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_maximized", "", NULL },
    { "unset_maximized", "", NULL },
    { "set_fullscreen", "o", (const struct wl_interface *[]){ &wl_output_interface } },
    { "unset_fullscreen", "", NULL },
    { "set_minimized", "", NULL },
};

static const struct wl_message xdg_toplevel_events[] = {
    { "configure", "iia", (const struct wl_interface *[]){ NULL, NULL, NULL } },
    { "close", "", NULL },
    { "configure_bounds", "ii", (const struct wl_interface *[]){ NULL, NULL } },
    { "wm_capabilities", "a", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface xdg_toplevel_interface = {
    "xdg_toplevel", 7,
    14, xdg_toplevel_requests, 4, xdg_toplevel_events
};

static inline void xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL, 0, 0);
}

static inline void xdg_toplevel_set_parent(struct xdg_toplevel *xdg_toplevel, struct xdg_toplevel *parent) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_PARENT, NULL, 0, 0, (struct wl_proxy*)parent);
}

static inline void xdg_toplevel_set_title(struct xdg_toplevel *xdg_toplevel, const char *title) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, NULL, 0, 0, title);
}

static inline void xdg_toplevel_set_app_id(struct xdg_toplevel *xdg_toplevel, const char *app_id) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_APP_ID, NULL, 0, 0, app_id);
}

static inline void xdg_toplevel_show_window_menu(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, int32_t x, int32_t y) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SHOW_WINDOW_MENU, NULL, 0, 0, (struct wl_proxy*)seat, serial, x, y);
}

static inline void xdg_toplevel_move(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_MOVE, NULL, 0, 0, (struct wl_proxy*)seat, serial);
}

static inline void xdg_toplevel_resize(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, uint32_t edges) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_RESIZE, NULL, 0, 0, (struct wl_proxy*)seat, serial, edges);
}

static inline void xdg_toplevel_set_max_size(struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_MAX_SIZE, NULL, 0, 0, width, height);
}

static inline void xdg_toplevel_set_min_size(struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_MIN_SIZE, NULL, 0, 0, width, height);
}

static inline void xdg_toplevel_set_maximized(struct xdg_toplevel *xdg_toplevel) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_MAXIMIZED, NULL, 0, 0);
}

static inline void xdg_toplevel_unset_maximized(struct xdg_toplevel *xdg_toplevel) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_UNSET_MAXIMIZED, NULL, 0, 0);
}

static inline void xdg_toplevel_set_fullscreen(struct xdg_toplevel *xdg_toplevel, struct wl_output *output) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL, 0, 0, (struct wl_proxy*)output);
}

static inline void xdg_toplevel_unset_fullscreen(struct xdg_toplevel *xdg_toplevel) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN, NULL, 0, 0);
}

static inline void xdg_toplevel_set_minimized(struct xdg_toplevel *xdg_toplevel) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_toplevel, XDG_TOPLEVEL_SET_MINIMIZED, NULL, 0, 0);
}

struct xdg_toplevel_listener {
    void (*configure)(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
    void (*close)(void *data, struct xdg_toplevel *xdg_toplevel);
    void (*configure_bounds)(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
    void (*wm_capabilities)(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
};

static inline int xdg_toplevel_add_listener(struct xdg_toplevel *xdg_toplevel,
    const struct xdg_toplevel_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy*)xdg_toplevel,
        (void (**)(void))listener, data);
}

enum xdg_popup_request {
    XDG_POPUP_DESTROY = 0,
    XDG_POPUP_GRAB = 1,
    XDG_POPUP_REPOSITION = 2,
    XDG_POPUP_REQUEST_COUNT = 3
};

enum xdg_popup_event {
    XDG_POPUP_CONFIGURE = 0,
    XDG_POPUP_POPUP_DONE = 1,
    XDG_POPUP_REPOSITIONED = 2,
    XDG_POPUP_EVENT_COUNT = 3
};

enum xdg_popup_ERROR {
    XDG_POPUP_ERROR_INVALID_GRAB = 0,
};

static const struct wl_message xdg_popup_requests[] = {
    { "destroy", "", NULL },
    { "grab", "ou", (const struct wl_interface *[]){ &wl_seat_interface, NULL } },
    { "reposition", "ou", (const struct wl_interface *[]){ &xdg_positioner_interface, NULL } },
};

static const struct wl_message xdg_popup_events[] = {
    { "configure", "iiii", (const struct wl_interface *[]){ NULL, NULL, NULL, NULL } },
    { "popup_done", "", NULL },
    { "repositioned", "u", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface xdg_popup_interface = {
    "xdg_popup", 7,
    3, xdg_popup_requests, 3, xdg_popup_events
};

static inline void xdg_popup_destroy(struct xdg_popup *xdg_popup) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_popup, XDG_POPUP_DESTROY, NULL, 0, 0);
}

static inline void xdg_popup_grab(struct xdg_popup *xdg_popup, struct wl_seat *seat, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_popup, XDG_POPUP_GRAB, NULL, 0, 0, (struct wl_proxy*)seat, serial);
}

static inline void xdg_popup_reposition(struct xdg_popup *xdg_popup, struct xdg_positioner *positioner, uint32_t token) {
    wl_proxy_marshal_flags((struct wl_proxy*)xdg_popup, XDG_POPUP_REPOSITION, NULL, 0, 0, (struct wl_proxy*)positioner, token);
}

struct xdg_popup_listener {
    void (*configure)(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y, int32_t width, int32_t height);
    void (*popup_done)(void *data, struct xdg_popup *xdg_popup);
    void (*repositioned)(void *data, struct xdg_popup *xdg_popup, uint32_t token);
};

static inline int xdg_popup_add_listener(struct xdg_popup *xdg_popup,
    const struct xdg_popup_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy*)xdg_popup,
        (void (**)(void))listener, data);
}

