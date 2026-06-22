#include "ui/dock_config.h"
#include "ui/x11_integration.h"
#include "ui/dock_ui.h"

DockPosition get_dock_position(void) {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "vaxp/dock", NULL);
    gchar *config_file = g_build_filename(config_dir, "dock_state.vaxp", NULL);
    gchar *contents = NULL;
    DockPosition pos = DOCK_POSITION_BOTTOM;

    if (!g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(config_dir, 0755);
        g_file_set_contents(config_file, "bottom", -1, NULL);
    }

    if (g_file_get_contents(config_file, &contents, NULL, NULL)) {
        g_strstrip(contents);
        if (g_ascii_strcasecmp(contents, "top") == 0) {
            pos = DOCK_POSITION_TOP;
        } else if (g_ascii_strcasecmp(contents, "left") == 0) {
            pos = DOCK_POSITION_LEFT;
        } else if (g_ascii_strcasecmp(contents, "right") == 0) {
            pos = DOCK_POSITION_RIGHT;
        }
        g_free(contents);
    }

    g_free(config_file);
    g_free(config_dir);
    return pos;
}

void apply_dock_position(void) {
    if (!main_window || !box || !system_separator) return;

    gboolean is_vertical = (current_dock_position == DOCK_POSITION_LEFT || current_dock_position == DOCK_POSITION_RIGHT);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(box), is_vertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(system_separator), is_vertical ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    if (is_vertical) {
        gtk_widget_set_margin_top(system_separator, 6);
        gtk_widget_set_margin_bottom(system_separator, 8);
        gtk_widget_set_margin_start(system_separator, 0);
        gtk_widget_set_margin_end(system_separator, 0);
        gtk_widget_set_size_request(system_separator, 28, 1);
        gtk_window_set_default_size(GTK_WINDOW(main_window), 60, -1);
    } else {
        gtk_widget_set_margin_start(system_separator, 6);
        gtk_widget_set_margin_end(system_separator, 8);
        gtk_widget_set_margin_top(system_separator, 0);
        gtk_widget_set_margin_bottom(system_separator, 0);
        gtk_widget_set_size_request(system_separator, 1, 28);
        gtk_window_set_default_size(GTK_WINDOW(main_window), -1, 60);
    }

    if (is_wayland_session) {
        if (current_dock_position == DOCK_POSITION_TOP) {
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, 2);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, 0);
        } else if (current_dock_position == DOCK_POSITION_LEFT) {
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, 2);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
        } else if (current_dock_position == DOCK_POSITION_RIGHT) {
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, 2);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
        } else {
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, FALSE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 2);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_TOP, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_LEFT, 0);
            gtk_layer_set_margin(GTK_WINDOW(main_window), GTK_LAYER_SHELL_EDGE_RIGHT, 0);
        }
    } else {
        GdkDisplay *display = gtk_widget_get_display(main_window);
        GdkRectangle primary_geom = {0};
        
        if (get_primary_monitor_geometry(display, &primary_geom)) {
            GtkAllocation alloc;
            gtk_widget_get_allocation(main_window, &alloc);
            int w = is_vertical ? 60 : alloc.width;
            int h = is_vertical ? alloc.height : 60;
            
            int x, y;
            if (current_dock_position == DOCK_POSITION_TOP) {
                x = primary_geom.x + (primary_geom.width - w) / 2;
                y = primary_geom.y + 2;
            } else if (current_dock_position == DOCK_POSITION_LEFT) {
                x = primary_geom.x + 2;
                y = primary_geom.y + (primary_geom.height - h) / 2;
            } else if (current_dock_position == DOCK_POSITION_RIGHT) {
                x = primary_geom.x + primary_geom.width - w - 2;
                y = primary_geom.y + (primary_geom.height - h) / 2;
            } else {
                x = primary_geom.x + (primary_geom.width - w) / 2;
                y = primary_geom.y + primary_geom.height - h - 2;
            }
            gtk_window_move(GTK_WINDOW(main_window), x, y);
        }
        
        on_dock_realize(main_window, NULL);
    }

    if (current_dock_position == DOCK_POSITION_TOP) {
        gtk_window_set_gravity(GTK_WINDOW(main_window), GDK_GRAVITY_NORTH);
    } else if (current_dock_position == DOCK_POSITION_LEFT) {
        gtk_window_set_gravity(GTK_WINDOW(main_window), GDK_GRAVITY_WEST);
    } else if (current_dock_position == DOCK_POSITION_RIGHT) {
        gtk_window_set_gravity(GTK_WINDOW(main_window), GDK_GRAVITY_EAST);
    } else {
        gtk_window_set_gravity(GTK_WINDOW(main_window), GDK_GRAVITY_SOUTH);
    }

    update_window_list();
}

void on_dock_config_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
    (void)monitor;
    (void)file;
    (void)other_file;
    (void)user_data;

    if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
        DockPosition new_pos = get_dock_position();
        if (new_pos != current_dock_position) {
            current_dock_position = new_pos;
            apply_dock_position();
        }
    }
}
