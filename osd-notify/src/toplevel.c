#include "osd_protocols.h"
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include <string.h>

extern struct wl_display *aether_display;

struct wl_seat *aether_seat = NULL;
struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;
GList *toplevel_handles = NULL;

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    char *app_id;
    char *title;
} ToplevelHandle;

static void handle_toplevel_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, const char *title) {
    ToplevelHandle *th = data;
    g_free(th->title);
    th->title = g_strdup(title);
}

static void handle_toplevel_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, const char *app_id) {
    ToplevelHandle *th = data;
    g_free(th->app_id);
    th->app_id = g_strdup(app_id);
}

static void handle_toplevel_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, struct wl_output *output) {}
static void handle_toplevel_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, struct wl_output *output) {}
static void handle_toplevel_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, struct wl_array *state) {}
static void handle_toplevel_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel) {}
static void handle_toplevel_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    ToplevelHandle *th = data;
    toplevel_handles = g_list_remove(toplevel_handles, th);
    g_free(th->app_id);
    g_free(th->title);
    zwlr_foreign_toplevel_handle_v1_destroy(toplevel);
    g_free(th);
}

const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = handle_toplevel_title,
    .app_id = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter, 
    .output_leave = handle_toplevel_output_leave,
    .state = handle_toplevel_state,
    .done = handle_toplevel_done,
    .closed = handle_toplevel_closed,
};

static void handle_toplevel_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager, struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    ToplevelHandle *th = g_malloc0(sizeof(ToplevelHandle));
    th->handle = toplevel;
    toplevel_handles = g_list_append(toplevel_handles, th);
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevel_handle_listener, th);
}

static void handle_toplevel_manager_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager) {}

const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_listener = {
    .toplevel = handle_toplevel_toplevel,
    .finished = handle_toplevel_manager_finished,
};

void osd_protocols_focus_toplevel(const char *app_id) {
    if (!toplevel_manager || !aether_seat || !app_id) return;

    gchar *lower_target = g_ascii_strdown(app_id, -1);
    for (GList *l = toplevel_handles; l != NULL; l = l->next) {
        ToplevelHandle *th = l->data;
        if (th->app_id) {
            gchar *lower_th = g_ascii_strdown(th->app_id, -1);
            if (g_str_has_prefix(lower_th, lower_target) || g_strstr_len(lower_th, -1, lower_target)) {
                zwlr_foreign_toplevel_handle_v1_activate(th->handle, aether_seat);
                g_free(lower_th);
                break;
            }
            g_free(lower_th);
        }
    }
    g_free(lower_target);
    if (aether_display) wl_display_flush(aether_display);
}
