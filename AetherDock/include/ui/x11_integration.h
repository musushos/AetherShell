#ifndef AETHERDOCK_X11_INTEGRATION_H
#define AETHERDOCK_X11_INTEGRATION_H

#include "ui/globals.h"

gboolean get_primary_monitor_geometry(GdkDisplay *display, GdkRectangle *geom);
void on_dock_realize(GtkWidget *widget, gpointer data);
void on_dock_window_destroy(GtkWidget *widget, gpointer user_data);

#endif // AETHERDOCK_X11_INTEGRATION_H
