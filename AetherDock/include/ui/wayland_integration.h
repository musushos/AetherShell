#ifndef AETHERDOCK_WAYLAND_INTEGRATION_H
#define AETHERDOCK_WAYLAND_INTEGRATION_H

#include "ui/globals.h"

void init_wayland_protocols(void);
gboolean flush_wayland_events(gpointer data);
void free_wayland_toplevel(gpointer data);
void wayland_schedule_refresh(void);
WaylandToplevel *wayland_toplevel_from_handle(struct zwlr_foreign_toplevel_handle_v1 *handle);

#endif // AETHERDOCK_WAYLAND_INTEGRATION_H
