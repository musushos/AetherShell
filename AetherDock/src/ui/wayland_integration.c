#include "ui/wayland_integration.h"
#include "ui/app_tracker.h"
#include "ui/dock_ui.h"

gboolean flush_wayland_events(gpointer data) {
    (void)data;

    if (!wl_display_conn) {
        return G_SOURCE_CONTINUE;
    }

    wl_display_dispatch_pending(wl_display_conn);
    wl_display_flush(wl_display_conn);
    return G_SOURCE_CONTINUE;
}

void free_wayland_toplevel(gpointer data) {
    WaylandToplevel *item = (WaylandToplevel *)data;

    if (!item) {
        return;
    }

    g_free(item->app_id);
    g_free(item->title);
    g_free(item);
}

void wayland_schedule_refresh(void) {
    if (is_wayland_session && box != NULL) {
        update_window_list();
    }
}

WaylandToplevel *wayland_toplevel_from_handle(struct zwlr_foreign_toplevel_handle_v1 *handle) {
    if (!wayland_toplevels) {
        return NULL;
    }

    return g_hash_table_lookup(wayland_toplevels, handle);
}

static void handle_toplevel_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *title) {
    (void)data;
    WaylandToplevel *item = wayland_toplevel_from_handle(handle);

    if (!item) {
        return;
    }

    g_free(item->title);
    item->title = g_strdup(title);
    wayland_schedule_refresh();
}

static void handle_toplevel_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *app_id) {
    (void)data;
    WaylandToplevel *item = wayland_toplevel_from_handle(handle);
    gchar *normalized_app_id;

    if (!item) {
        return;
    }

    normalized_app_id = normalize_app_id((app_id && *app_id) ? app_id : "unknown");
    g_free(item->app_id);
    item->app_id = normalized_app_id;
    wayland_schedule_refresh();
}

static void handle_toplevel_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_output *output) {
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_toplevel_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_output *output) {
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_toplevel_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_array *state) {
    (void)data;
    WaylandToplevel *item = wayland_toplevel_from_handle(handle);
    uint32_t *entry;
    size_t count;

    if (!item) {
        return;
    }

    item->state_flags = 0;
    entry = state->data;
    count = state->size / sizeof(uint32_t);

    for (size_t i = 0; i < count; i++) {
        if (entry[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
            item->state_flags |= WAYLAND_TOPLEVEL_STATE_ACTIVATED;
        } else if (entry[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) {
            item->state_flags |= WAYLAND_TOPLEVEL_STATE_MINIMIZED;
        } else if (entry[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) {
            item->state_flags |= WAYLAND_TOPLEVEL_STATE_MAXIMIZED;
        }
    }

    wayland_schedule_refresh();
}

static void handle_toplevel_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    (void)data;
    (void)handle;
    wayland_schedule_refresh();
}

static void handle_toplevel_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
    (void)data;

    if (!wayland_toplevels) {
        return;
    }

    g_hash_table_remove(wayland_toplevels, handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    wayland_schedule_refresh();
}

static void handle_toplevel_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct zwlr_foreign_toplevel_handle_v1 *parent) {
    (void)data;
    (void)handle;
    (void)parent;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_listener = {
    .title = handle_toplevel_title,
    .app_id = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter,
    .output_leave = handle_toplevel_output_leave,
    .state = handle_toplevel_state,
    .done = handle_toplevel_done,
    .closed = handle_toplevel_closed,
    .parent = handle_toplevel_parent,
};

static void handle_manager_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager, struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    WaylandToplevel *item;

    (void)data;
    (void)manager;

    item = g_new0(WaylandToplevel, 1);
    item->handle = toplevel;
    item->app_id = normalize_app_id("unknown");

    g_hash_table_insert(wayland_toplevels, toplevel, item);
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevel_listener, NULL);
    wayland_schedule_refresh();
}

static void handle_manager_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager) {
    (void)data;
    (void)manager;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};

static void registry_add_object(void *data, struct wl_registry *registry,
                                uint32_t name, const char *interface, uint32_t version) {
    (void)data;

    if (strcmp(interface, wl_seat_interface.name) == 0) {
        wl_seat_obj = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        return;
    }

    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        uint32_t bind_version = version < 1 ? version : 1;
        toplevel_manager = wl_registry_bind(registry, name,
                                            &zwlr_foreign_toplevel_manager_v1_interface,
                                            bind_version);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_add_object,
    .global_remove = registry_remove_object,
};

void init_wayland_protocols(void) {
    struct wl_registry *registry;

    if (!is_wayland_session) {
        return;
    }

    wl_display_conn = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    if (!wl_display_conn) {
        g_warning("Wayland session detected, but wl_display is unavailable.");
        return;
    }

    registry = wl_display_get_registry(wl_display_conn);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(wl_display_conn);
    wl_registry_destroy(registry);

    if (!toplevel_manager) {
        g_warning("Compositor does not expose wlr-foreign-toplevel-management. "
                  "Wayland task/window integration is not ready yet.");
        return;
    }

    wayland_toplevels = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_wayland_toplevel);
    zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager, &manager_listener, NULL);
    wl_display_roundtrip(wl_display_conn);
    wayland_event_source_id = g_timeout_add(80, flush_wayland_events, NULL);
}
