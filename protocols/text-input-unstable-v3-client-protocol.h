/* Auto-generated from text-input-unstable-v3.xml -- do not edit. */
#pragma once

#include <wayland-client.h>

extern const struct wl_interface zwp_text_input_v3_interface;
extern const struct wl_interface zwp_text_input_manager_v3_interface;

enum zwp_text_input_v3_request {
    ZWP_TEXT_INPUT_V3_DESTROY = 0,
    ZWP_TEXT_INPUT_V3_ENABLE = 1,
    ZWP_TEXT_INPUT_V3_DISABLE = 2,
    ZWP_TEXT_INPUT_V3_SET_SURROUNDING_TEXT = 3,
    ZWP_TEXT_INPUT_V3_SET_TEXT_CHANGE_CAUSE = 4,
    ZWP_TEXT_INPUT_V3_SET_CONTENT_TYPE = 5,
    ZWP_TEXT_INPUT_V3_SET_CURSOR_RECTANGLE = 6,
    ZWP_TEXT_INPUT_V3_COMMIT = 7,
    ZWP_TEXT_INPUT_V3_SET_AVAILABLE_ACTIONS = 8,
    ZWP_TEXT_INPUT_V3_SHOW_INPUT_PANEL = 9,
    ZWP_TEXT_INPUT_V3_HIDE_INPUT_PANEL = 10,
    ZWP_TEXT_INPUT_V3_REQUEST_COUNT = 11
};

enum zwp_text_input_v3_event {
    ZWP_TEXT_INPUT_V3_ENTER = 0,
    ZWP_TEXT_INPUT_V3_LEAVE = 1,
    ZWP_TEXT_INPUT_V3_PREEDIT_STRING = 2,
    ZWP_TEXT_INPUT_V3_COMMIT_STRING = 3,
    ZWP_TEXT_INPUT_V3_DELETE_SURROUNDING_TEXT = 4,
    ZWP_TEXT_INPUT_V3_DONE = 5,
    ZWP_TEXT_INPUT_V3_ACTION = 6,
    ZWP_TEXT_INPUT_V3_LANGUAGE = 7,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT = 8,
    ZWP_TEXT_INPUT_V3_EVENT_COUNT = 9
};

enum zwp_text_input_v3_CHANGE_CAUSE {
    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD = 0,
    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER = 1,
};

enum zwp_text_input_v3_CONTENT_HINT {
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE = 0x0,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION = 0x1,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK = 0x2,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION = 0x4,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE = 0x8,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE = 0x10,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE = 0x20,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT = 0x40,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA = 0x80,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_LATIN = 0x100,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_MULTILINE = 0x200,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_ON_SCREEN_INPUT_PROVIDED = 0x400,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_NO_EMOJI = 0x800,
    ZWP_TEXT_INPUT_V3_CONTENT_HINT_PREEDIT_SHOWN = 0x1000,
};

enum zwp_text_input_v3_CONTENT_PURPOSE {
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL = 0,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA = 1,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS = 2,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER = 3,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE = 4,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL = 5,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL = 6,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME = 7,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD = 8,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN = 9,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE = 10,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME = 11,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME = 12,
    ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL = 13,
};

enum zwp_text_input_v3_ERROR {
    ZWP_TEXT_INPUT_V3_ERROR_INVALID_ACTION = 0,
};

enum zwp_text_input_v3_ACTION {
    ZWP_TEXT_INPUT_V3_ACTION_NONE = 0,
    ZWP_TEXT_INPUT_V3_ACTION_SUBMIT = 1,
};

enum zwp_text_input_v3_PREEDIT_HINT {
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_WHOLE = 1,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_SELECTION = 2,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_PREDICTION = 3,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_PREFIX = 4,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_SUFFIX = 5,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_SPELLING_ERROR = 6,
    ZWP_TEXT_INPUT_V3_PREEDIT_HINT_COMPOSE_ERROR = 7,
};

static const struct wl_message zwp_text_input_v3_requests[] = {
    { "destroy", "", NULL },
    { "enable", "", NULL },
    { "disable", "", NULL },
    { "set_surrounding_text", "sii", (const struct wl_interface *[]){ NULL, NULL, NULL } },
    { "set_text_change_cause", "u", (const struct wl_interface *[]){ NULL } },
    { "set_content_type", "uu", (const struct wl_interface *[]){ NULL, NULL } },
    { "set_cursor_rectangle", "iiii", (const struct wl_interface *[]){ NULL, NULL, NULL, NULL } },
    { "commit", "", NULL },
    { "set_available_actions", "a", (const struct wl_interface *[]){ NULL } },
    { "show_input_panel", "", NULL },
    { "hide_input_panel", "", NULL },
};

static const struct wl_message zwp_text_input_v3_events[] = {
    { "enter", "o", (const struct wl_interface *[]){ &wl_surface_interface } },
    { "leave", "o", (const struct wl_interface *[]){ &wl_surface_interface } },
    { "preedit_string", "sii", (const struct wl_interface *[]){ NULL, NULL, NULL } },
    { "commit_string", "s", (const struct wl_interface *[]){ NULL } },
    { "delete_surrounding_text", "uu", (const struct wl_interface *[]){ NULL, NULL } },
    { "done", "u", (const struct wl_interface *[]){ NULL } },
    { "action", "uu", (const struct wl_interface *[]){ NULL, NULL } },
    { "language", "s", (const struct wl_interface *[]){ NULL } },
    { "preedit_hint", "uuu", (const struct wl_interface *[]){ NULL, NULL, NULL } },
};

const struct wl_interface zwp_text_input_v3_interface = {
    "zwp_text_input_v3", 2,
    11, zwp_text_input_v3_requests, 9, zwp_text_input_v3_events
};

static inline void zwp_text_input_v3_destroy(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_DESTROY, NULL, 0, 0);
}

static inline void zwp_text_input_v3_enable(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_ENABLE, NULL, 0, 0);
}

static inline void zwp_text_input_v3_disable(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_DISABLE, NULL, 0, 0);
}

static inline void zwp_text_input_v3_set_surrounding_text(struct zwp_text_input_v3 *zwp_text_input_v3, const char *text, int32_t cursor, int32_t anchor) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SET_SURROUNDING_TEXT, NULL, 0, 0, text, cursor, anchor);
}

static inline void zwp_text_input_v3_set_text_change_cause(struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t cause) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SET_TEXT_CHANGE_CAUSE, NULL, 0, 0, cause);
}

static inline void zwp_text_input_v3_set_content_type(struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t hint, uint32_t purpose) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SET_CONTENT_TYPE, NULL, 0, 0, hint, purpose);
}

static inline void zwp_text_input_v3_set_cursor_rectangle(struct zwp_text_input_v3 *zwp_text_input_v3, int32_t x, int32_t y, int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SET_CURSOR_RECTANGLE, NULL, 0, 0, x, y, width, height);
}

static inline void zwp_text_input_v3_commit(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_COMMIT, NULL, 0, 0);
}

static inline void zwp_text_input_v3_set_available_actions(struct zwp_text_input_v3 *zwp_text_input_v3, struct wl_array *available_actions) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SET_AVAILABLE_ACTIONS, NULL, 0, 0, available_actions);
}

static inline void zwp_text_input_v3_show_input_panel(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_SHOW_INPUT_PANEL, NULL, 0, 0);
}

static inline void zwp_text_input_v3_hide_input_panel(struct zwp_text_input_v3 *zwp_text_input_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_v3, ZWP_TEXT_INPUT_V3_HIDE_INPUT_PANEL, NULL, 0, 0);
}

struct zwp_text_input_v3_listener {
    void (*enter)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, struct wl_surface *surface);
    void (*leave)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, struct wl_surface *surface);
    void (*preedit_string)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, const char *text, int32_t cursor_begin, int32_t cursor_end);
    void (*commit_string)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, const char *text);
    void (*delete_surrounding_text)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t before_length, uint32_t after_length);
    void (*done)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t serial);
    void (*action)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t action, uint32_t serial);
    void (*language)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, const char *language);
    void (*preedit_hint)(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t start, uint32_t end, uint32_t hint);
};

static inline int zwp_text_input_v3_add_listener(struct zwp_text_input_v3 *zwp_text_input_v3,
    const struct zwp_text_input_v3_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy*)zwp_text_input_v3,
        (void (**)(void))listener, data);
}

enum zwp_text_input_manager_v3_request {
    ZWP_TEXT_INPUT_MANAGER_V3_DESTROY = 0,
    ZWP_TEXT_INPUT_MANAGER_V3_GET_TEXT_INPUT = 1,
    ZWP_TEXT_INPUT_MANAGER_V3_REQUEST_COUNT = 2
};

static const struct wl_message zwp_text_input_manager_v3_requests[] = {
    { "destroy", "", NULL },
    { "get_text_input", "no", (const struct wl_interface *[]){ &zwp_text_input_v3_interface, &wl_seat_interface } },
};

const struct wl_interface zwp_text_input_manager_v3_interface = {
    "zwp_text_input_manager_v3", 2,
    2, zwp_text_input_manager_v3_requests, 0, NULL
};

static inline void zwp_text_input_manager_v3_destroy(struct zwp_text_input_manager_v3 *zwp_text_input_manager_v3) {
    wl_proxy_marshal_flags((struct wl_proxy*)zwp_text_input_manager_v3, ZWP_TEXT_INPUT_MANAGER_V3_DESTROY, NULL, 0, 0);
}

static inline struct zwp_text_input_v3 *zwp_text_input_manager_v3_get_text_input(struct zwp_text_input_manager_v3 *zwp_text_input_manager_v3, struct wl_seat *seat) {
    return (struct zwp_text_input_v3 *)wl_proxy_marshal_flags(
        (struct wl_proxy*)zwp_text_input_manager_v3, ZWP_TEXT_INPUT_MANAGER_V3_GET_TEXT_INPUT, &zwp_text_input_v3_interface, 1, 0, (struct wl_proxy*)seat);
}

