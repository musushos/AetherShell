#include "ui/x11_integration.h"

gboolean dock_has_available_monitor(void) {
    GdkDisplay *display = gdk_display_get_default();

    if (!display) {
        return FALSE;
    }

    if (gdk_display_get_primary_monitor(display)) {
        return TRUE;
    }

    return gdk_display_get_n_monitors(display) > 0;
}

gboolean get_primary_monitor_geometry(GdkDisplay *display, GdkRectangle *geom) {
    GdkMonitor *monitor = NULL;

    if (!display || !geom) {
        return FALSE;
    }

    monitor = gdk_display_get_primary_monitor(display);
    if (!monitor && gdk_display_get_n_monitors(display) > 0) {
        monitor = gdk_display_get_monitor(display, 0);
    }
    if (!monitor) {
        return FALSE;
    }

    gdk_monitor_get_geometry(monitor, geom);
    return TRUE;
}

gboolean restart_dock_process(void) {
    GError *error = NULL;
    gchar *argv[] = { dock_executable_path, NULL };

    if (!dock_executable_path || dock_executable_path[0] == '\0') {
        g_warning("[AetherDock] Cannot restart: executable path is unavailable");
        return FALSE;
    }

    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
        g_warning("[AetherDock] Failed to restart dock: %s",
                  error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    return TRUE;
}

gboolean try_recover_dock(gpointer data) {
    (void)data;

    if (!dock_has_available_monitor()) {
        return G_SOURCE_CONTINUE;
    }

    if (restart_dock_process()) {
        recovery_source_id = 0;
        gtk_main_quit();
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

void on_dock_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;

    if (recovery_source_id == 0) {
        recovery_source_id = g_timeout_add(1000, try_recover_dock, NULL);
    }
}

void on_dock_realize(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    if (!gdk_window) {
        return;
    }

    if (is_wayland_session) {
        gdk_window_set_type_hint(gdk_window, GDK_WINDOW_TYPE_HINT_DOCK);
        return;
    }

    GdkDisplay *display = gdk_window_get_display(gdk_window);
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    
    /* Set window as dock type */
    gdk_window_set_type_hint(gdk_window, GDK_WINDOW_TYPE_HINT_DOCK);
    
    gulong strut[12] = {0};
    gulong simple_strut[4] = {0};

    if (current_dock_position == DOCK_POSITION_TOP) {
        strut[2] = 55; // top
        strut[8] = geometry.x; // top_start_x
        strut[9] = geometry.x + geometry.width; // top_end_x
        simple_strut[2] = 55;
    } else if (current_dock_position == DOCK_POSITION_LEFT) {
        strut[0] = 55; // left
        strut[4] = geometry.y; // left_start_y
        strut[5] = geometry.y + geometry.height; // left_end_y
        simple_strut[0] = 55;
    } else if (current_dock_position == DOCK_POSITION_RIGHT) {
        strut[1] = 55; // right
        strut[6] = geometry.y; // right_start_y
        strut[7] = geometry.y + geometry.height; // right_end_y
        simple_strut[1] = 55;
    } else {
        strut[3] = 55;  /* bottom */
        strut[10] = geometry.x;  /* bottom_start_x */
        strut[11] = geometry.x + geometry.width;  /* bottom_end_x */
        simple_strut[3] = 55;
    }
    
    gdk_property_change(gdk_window, 
                       gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)strut, 12);
    
    /* Also set _NET_WM_STRUT for older window managers */
    gdk_property_change(gdk_window,
                       gdk_atom_intern("_NET_WM_STRUT", FALSE),
                       gdk_atom_intern("CARDINAL", FALSE),
                       32, GDK_PROP_MODE_REPLACE,
                       (guchar *)simple_strut, 4);
}
