/* xdg-decoration-unstable-v1 client protocol bindings for campello_widgets.
 * Hand-written to match the project's existing protocol header style.
 * Protocol: https://gitlab.freedesktop.org/wayland/wayland-protocols
 */
#pragma once

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

extern const struct wl_interface zxdg_decoration_manager_v1_interface;
extern const struct wl_interface zxdg_toplevel_decoration_v1_interface;

/* -------------------------------------------------------------------------
 * zxdg_decoration_manager_v1
 * -------------------------------------------------------------------- */

enum zxdg_decoration_manager_v1_request {
    ZXDG_DECORATION_MANAGER_V1_DESTROY                  = 0,
    ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION  = 1,
};

static const struct wl_message zxdg_decoration_manager_v1_requests[] = {
    { "destroy",                   "",   NULL },
    { "get_toplevel_decoration",   "no", (const struct wl_interface *[]){
        &zxdg_toplevel_decoration_v1_interface,
        &xdg_toplevel_interface } },
};

const struct wl_interface zxdg_decoration_manager_v1_interface = {
    "zxdg_decoration_manager_v1", 1,
    2, zxdg_decoration_manager_v1_requests,
    0, NULL
};

static inline void
zxdg_decoration_manager_v1_destroy(struct zxdg_decoration_manager_v1 *mgr) {
    wl_proxy_marshal_flags((struct wl_proxy *)mgr,
        ZXDG_DECORATION_MANAGER_V1_DESTROY, NULL, 0, 0);
}

static inline struct zxdg_toplevel_decoration_v1 *
zxdg_decoration_manager_v1_get_toplevel_decoration(
    struct zxdg_decoration_manager_v1 *mgr,
    struct xdg_toplevel              *toplevel)
{
    return (struct zxdg_toplevel_decoration_v1 *)wl_proxy_marshal_flags(
        (struct wl_proxy *)mgr,
        ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION,
        &zxdg_toplevel_decoration_v1_interface, 1, 0,
        (struct wl_proxy *)toplevel);
}

/* -------------------------------------------------------------------------
 * zxdg_toplevel_decoration_v1
 * -------------------------------------------------------------------- */

enum zxdg_toplevel_decoration_v1_request {
    ZXDG_TOPLEVEL_DECORATION_V1_DESTROY    = 0,
    ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE   = 1,
    ZXDG_TOPLEVEL_DECORATION_V1_UNSET_MODE = 2,
};

enum zxdg_toplevel_decoration_v1_event {
    ZXDG_TOPLEVEL_DECORATION_V1_CONFIGURE = 0,
};

enum zxdg_toplevel_decoration_v1_mode {
    ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE = 1,
    ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE = 2,
};

static const struct wl_message zxdg_toplevel_decoration_v1_requests[] = {
    { "destroy",    "",  NULL },
    { "set_mode",   "u", (const struct wl_interface *[]){ NULL } },
    { "unset_mode", "",  NULL },
};

static const struct wl_message zxdg_toplevel_decoration_v1_events[] = {
    { "configure", "u", (const struct wl_interface *[]){ NULL } },
};

const struct wl_interface zxdg_toplevel_decoration_v1_interface = {
    "zxdg_toplevel_decoration_v1", 1,
    3, zxdg_toplevel_decoration_v1_requests,
    1, zxdg_toplevel_decoration_v1_events
};

struct zxdg_toplevel_decoration_v1_listener {
    void (*configure)(void                               *data,
                      struct zxdg_toplevel_decoration_v1 *decoration,
                      uint32_t                            mode);
};

static inline int
zxdg_toplevel_decoration_v1_add_listener(
    struct zxdg_toplevel_decoration_v1         *decoration,
    const struct zxdg_toplevel_decoration_v1_listener *listener,
    void                                       *data)
{
    return wl_proxy_add_listener((struct wl_proxy *)decoration,
        (void (**)(void))listener, data);
}

static inline void
zxdg_toplevel_decoration_v1_destroy(struct zxdg_toplevel_decoration_v1 *decoration) {
    wl_proxy_marshal_flags((struct wl_proxy *)decoration,
        ZXDG_TOPLEVEL_DECORATION_V1_DESTROY, NULL, 0, 0);
}

static inline void
zxdg_toplevel_decoration_v1_set_mode(struct zxdg_toplevel_decoration_v1 *decoration,
                                      uint32_t mode) {
    wl_proxy_marshal_flags((struct wl_proxy *)decoration,
        ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE, NULL, 0, 0, mode);
}

static inline void
zxdg_toplevel_decoration_v1_unset_mode(struct zxdg_toplevel_decoration_v1 *decoration) {
    wl_proxy_marshal_flags((struct wl_proxy *)decoration,
        ZXDG_TOPLEVEL_DECORATION_V1_UNSET_MODE, NULL, 0, 0);
}
